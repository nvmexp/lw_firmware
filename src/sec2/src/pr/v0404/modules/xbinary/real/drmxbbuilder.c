/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_DRMXBBUILDER_C
#include <drmxb.h>
#include <drmxbbuilder.h>
#include <drmerr.h>
#include <drmbytemanip.h>
#include <oembyteorder.h>
#include <drmmathsafe.h>
#include <drmstkalloc.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#if defined(SEC_COMPILE)

static DRM_BOOL   DRM_CALL _DRM_XB_IsDuplicateAllowed(
    __in_ecount( 1 )  const DRM_XB_BUILDER_LISTNODE          *f_plistnode,
    __in              const DRM_XB_FORMAT_DESCRIPTION        *f_pformat );

static DRM_RESULT DRM_CALL _DRM_XB_GetObjectLength(
    __in                    DRM_WORD                          f_wType,
    __in_opt          const DRM_VOID                         *f_pvObject,
    __in              const DRM_XB_FORMAT_DESCRIPTION        *f_pformat,
    __out                   DRM_DWORD                        *f_pdwLength );

static DRM_RESULT DRM_CALL _DRM_XB_GetStructLength(
    __in              const DRM_XB_FORMAT_DESCRIPTION        *f_pFormat,
    __in              const DRM_VOID                         *f_pvObject,
    __in              const DRM_XB_ELEMENT_DESCRIPTION       *f_pStructList,
    __out                   DRM_DWORD                        *f_pdwLength );

static DRM_RESULT DRM_CALL _DRM_XB_Serialize_ElementList(
    __in_ecount( 1 )              const DRM_XB_BUILDER_CONTEXT_INTERNAL    *f_pcontextBuilder,
    __in                                DRM_DWORD                           f_cElements,
    __in_ecount( f_cElements )    const DRM_XB_ELEMENT_DESCRIPTION         *f_pElements,
    __in_ecount( 1 )              const DRM_VOID                           *f_pvObject,
    __inout_bcount( f_cbBuffer )        DRM_BYTE                           *f_pbBuffer,
    __in                                DRM_DWORD                           f_cbBuffer,
    __inout_ecount( 1 )                 DRM_DWORD                          *f_piBuffer  )
GCC_ATTRIB_NO_STACK_PROTECT();


static DRM_NO_INLINE DRM_BOOL _DRM_XB_IsNativeObject( __in DRM_WORD f_wType, __in const DRM_XB_FORMAT_DESCRIPTION *f_pFormat )
{
    /* Note: Internal function: No need to check input */

    if( _XB_IsKnownObjectType(f_pFormat, f_wType) )
    {
        if( ! ( f_pFormat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pFormat, f_wType)].wFlags & XB_FLAGS_ALLOW_EXTERNAL_PARSE ) )
        {
            return TRUE;
        }
    }

    return FALSE;
}

static DRM_NO_INLINE DRM_BOOL DRM_CALL _DRM_XB_IsDuplicateAllowed( __in_ecount( 1 ) const DRM_XB_BUILDER_LISTNODE *f_plistnode, __in const DRM_XB_FORMAT_DESCRIPTION *f_pFormat )
{
    /* Note: Internal function: No need to check input */
    DRM_BOOL fRetVal = FALSE;

    if( _DRM_XB_IsNativeObject( f_plistnode->Node.wType, f_pFormat ) )
    {
        fRetVal =  f_pFormat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pFormat, f_plistnode->Node.wType)].fDuplicateAllowed;
    }
    else
    {
        const DRM_XB_BUILDER_UNKNOWNOBJECT_LISTNODE *pUnkNode = DRM_REINTERPRET_CONST_CAST( const DRM_XB_BUILDER_UNKNOWNOBJECT_LISTNODE, f_plistnode );
        fRetVal = pUnkNode->fDuplicateAllowed;
    }

    return fRetVal;
}

/******************************************************************************
**
** Function :   _DRM_XB_GetParent
**
** Synopsis :   Returns the parent object type for a given node.
**
** Arguments :  f_plistnode - Node for which parent needs to be found
**              f_pFormat   - Format discription of the XBinary type
**
******************************************************************************/

static DRM_NO_INLINE DRM_WORD DRM_CALL _DRM_XB_GetParent(
    __in_ecount( 1 )    const DRM_XB_BUILDER_LISTNODE           *f_plistnode,
    __in                const DRM_XB_FORMAT_DESCRIPTION         *f_pFormat )
{
    /* Note: Internal function: No need to check input */
    DRM_WORD    wRetVal = XB_OBJECT_TYPE_ILWALID;

    if( _DRM_XB_IsNativeObject( f_plistnode->Node.wType, f_pFormat ) )
    {
        wRetVal = f_pFormat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pFormat, f_plistnode->Node.wType)].wParent;
    }
    else
    {
        const DRM_XB_BUILDER_UNKNOWNOBJECT_LISTNODE *pUnkNode = DRM_REINTERPRET_CONST_CAST( const DRM_XB_BUILDER_UNKNOWNOBJECT_LISTNODE, f_plistnode );
        wRetVal = pUnkNode->wParent;
    }

    return wRetVal;
}

/******************************************************************************
**
** Function :   _DRM_XB_GetBuilderNode
**
** Synopsis :   Returns the builder node for a given XB object type.
**              Returns null if object is not found
**
** Arguments :  f_pcontextBuilder - builder context
**              f_wType           - object type
**
******************************************************************************/

DRM_NO_INLINE DRM_XB_BUILDER_NODE* _DRM_XB_GetBuilderNode(
    __in_ecount( 1 ) const DRM_XB_BUILDER_CONTEXT_INTERNAL *f_pcontextBuilder,
    __in                   DRM_WORD                         f_wType )
{
    /* Note: Internal function: No need to check input */
    DRM_XB_BUILDER_NODE *pNode = NULL;

    if( _DRM_XB_IsNativeObject( f_wType, f_pcontextBuilder->pformat ) )
    {
        pNode = f_pcontextBuilder->rgpObjectNodes[_XB_MapObjectTypeToEntryDescriptionIndex(f_pcontextBuilder->pformat, f_wType)];
    }
    else
    {
        DRM_XB_BUILDER_UNKNOWNOBJECT_LISTNODE *pUnkList = f_pcontextBuilder->pUnknownObjects;
        while( pUnkList != NULL )
        {
            if( pUnkList->listNode.Node.wType == f_wType )
            {
                pNode = &( pUnkList->listNode.Node );
                break;
            }
            pUnkList = pUnkList->pNext;

        }
    }
    return pNode;
}

/******************************************************************************
**
** Function :   _DRM_XB_AddEstimatedAlignmentSize
**
** Synopsis :   Determines the possible size required by aligning the blob,
**              unknown object or struct list data to the alignment value
**              indicated by the format header.
**
** Arguments :  f_pFormat       - The XBinary format description.
**              f_eElementType  - The element type to be aligned.
**              f_pcbLength     - The current length of the object.
**
******************************************************************************/

static DRM_RESULT DRM_CALL _DRM_XB_AddEstimatedAlignmentSize(
         __in            const DRM_XB_FORMAT_DESCRIPTION      *f_pFormat,
         __in                  XB_ELEMENT_BASE_TYPE            f_eElementType,
         __inout_ecount( 1 )   DRM_DWORD                      *f_pcbLength )
{
    DRM_RESULT dr = DRM_SUCCESS;

    AssertChkArg( f_pFormat   != NULL );
    AssertChkArg( f_pcbLength != NULL );
    AssertChkArg( f_eElementType == XB_ELEMENT_TYPE_BYTEARRAY      ||
                  f_eElementType == XB_ELEMENT_TYPE_UNKNOWN_OBJECT ||
                  f_eElementType == XB_ELEMENT_TYPE_STRUCTLIST      );

    if( f_pFormat->pHeaderDescription != NULL &&
        f_pFormat->pHeaderDescription->eAlign != XB_ALIGN_1_BYTE )
    {
        DRM_XB_ALIGNMENT eAlign = f_pFormat->pHeaderDescription->eAlign;
        DRMASSERT( eAlign == XB_ALIGN_8_BYTE || eAlign == XB_ALIGN_16_BYTE );
        ChkDR( DRM_DWordAddSame( f_pcbLength, eAlign ) );
    }

 ErrorExit:
    return dr;
 }

/******************************************************************************
**
** Function :   _DRM_XB_GetByteArrayLength
**
** Synopsis :   Estimates the serialized size of a DRM_XB_BYTEARRAY object (including all
**              entries).
**
** Arguments :
**              [f_pFormat     ] : The XBinary format structure.
**              [f_pvBaseObject] : A pointer to the object that contains the provided
**                                 byte array (f_pvObject).
**              [f_pvObject    ] : The target byte array object for which the size will
**                                 be callwlated.
**              [f_pElement    ] : The element description for the byte array.
**              [f_pdwLength   ] : The serialized size of the byte array.
**
** Returns:
**              Returns DRM_SUCCESS on success
**
******************************************************************************/
static DRM_RESULT DRM_CALL _DRM_XB_GetByteArrayLength(
    __in            const DRM_XB_FORMAT_DESCRIPTION      *f_pFormat,
    __in_opt        const DRM_VOID                       *f_pvBaseObject,
    __in_opt        const DRM_VOID                       *f_pvObject,
    __in            const DRM_XB_ELEMENT_DESCRIPTION     *f_pElement,
    __inout_ecount( 1 )   DRM_DWORD                      *f_pcbLength )
{
    DRM_RESULT dr           = DRM_SUCCESS;
    DRM_DWORD  cbLength     = 0;
    DRM_BOOL   fAddDataSize = TRUE;
    DRM_DWORD  cbDelta      = 0;

    switch( f_pElement->xbSize.eSizeType )
    {
        case XB_SIZE_IS_RELATIVE_WORD:
            cbDelta = sizeof( DRM_WORD );
            break;

        case XB_SIZE_IS_RELATIVE_DWORD:
            cbDelta = sizeof( DRM_DWORD );
            break;

        case XB_SIZE_IS_ABSOLUTE:
            cbDelta = f_pElement->xbSize.wMinSize;
            fAddDataSize = FALSE;
            break;

        case XB_SIZE_IS_REMAINING_DATA_IN_OBJECT:
            /* No size is required because the object data fills the element */
            break;

        case XB_SIZE_IS_DATA_IN_OBJECT_FOR_WEAKREF: __fallthrough;
        case XB_SIZE_IS_ELEMENT_COUNT:
            /* Not serialized */
            goto ErrorExit;

        case XB_SIZE_IS_RELATIVE_DWORD_IN_BITS:
            cbDelta   = sizeof(DRM_DWORD);
            break;

        case XB_SIZE_IS_WORD_IN_BITS_ANOTHER_STRUCT_MEMBER: __fallthrough;
        case XB_SIZE_IS_WORD_IN_ANOTHER_STRUCT_MEMBER:      __fallthrough;
        case XB_SIZE_IS_DWORD_IN_ANOTHER_STRUCT_MEMBER:
            /* Uses default values */
            break;

        default:
            DRMASSERT(FALSE);
            fAddDataSize = FALSE;
            break;
    }

    /* Leave room for padding if needed */
    cbLength = ( DRM_DWORD )f_pElement->xbSize.wPadding;
    ChkDR( DRM_DWordAddSame( &cbLength, cbDelta ) );

    if( fAddDataSize )
    {
        DRM_DWORD dwSize;

        ChkArg( f_pvObject != NULL );

        dwSize = ( ( DRM_XB_BYTEARRAY * )f_pvObject )->cbData;

        ChkDR( DRM_DWordAddSame( &cbLength, dwSize ) );
    }

    ChkDR( _DRM_XB_AddEstimatedAlignmentSize( f_pFormat, f_pElement->eElementType, &cbLength ) );

    *f_pcbLength = cbLength;

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   _DRM_XB_GetDwordListLength
**
** Synopsis :   Estimates the serialized size of a DRM_XB_DWORDLIST object (including all
**              entries).
**
** Arguments :
**              [f_pFormat     ] : The XBinary format structure.
**              [f_pvObject    ] : The target DWORD list object for which the size will
**                                 be callwlated.
**              [f_pElement    ] : The element description for the target DWORD list.
**              [f_pdwLength   ] : The serialized size of the DWORD list.
**
** Returns:
**              Returns DRM_SUCCESS on success
**
******************************************************************************/
static DRM_RESULT DRM_CALL _DRM_XB_GetDwordListLength(
    __in            const DRM_XB_FORMAT_DESCRIPTION      *f_pFormat,
    __in            const DRM_VOID                       *f_pvObject,
    __in            const DRM_XB_ELEMENT_DESCRIPTION     *f_pElement,
    __inout_ecount( 1 )   DRM_DWORD                      *f_pcbLength )
{
    DRM_RESULT dr       = DRM_SUCCESS;
    DRM_DWORD  cbLength = 0;
    DRM_DWORD  dwResult = 0;

    ChkArg( f_pvObject != NULL );
    switch( f_pElement->xbSize.eSizeType )
    {
        case XB_SIZE_IS_RELATIVE_WORD:
            ChkDR( DRM_DWordAddSame( &cbLength, sizeof( DRM_WORD ) ) );
            break;

        case XB_SIZE_IS_WORD_IN_ANOTHER_STRUCT_MEMBER: __fallthrough;
        case XB_SIZE_IS_DWORD_IN_ANOTHER_STRUCT_MEMBER:
            /* Count is in the containing structure so do add its length */
            break;

        default:
            ChkDR( DRM_DWordAddSame( &cbLength, sizeof( DRM_DWORD ) ) );
            break;
    }
    ChkDR( DRM_DWordMult( ( ( DRM_XB_DWORDLIST * )f_pvObject )->cDWORDs,  sizeof( DRM_DWORD ), &dwResult ) );
    ChkDR( DRM_DWordAddSame( &cbLength, dwResult ) );

    *f_pcbLength = cbLength;

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   _DRM_XB_GetStructListLength
**
** Synopsis :   Estimates the serialized size of a struct list object (including all
**              entries).
**
** Arguments :
**              [f_pFormat     ] : The XBinary format structure.
**              [f_pvBaseObject] : A pointer to the object that contains the provided
**                                 struct list (f_pvObject).
**              [f_pElement    ] : The element description for the target struct list.
**              [f_pdwLength   ] : The serialized size of the struct list.
**
** Returns:
**              Returns DRM_SUCCESS on success
**
******************************************************************************/
static DRM_RESULT DRM_CALL _DRM_XB_GetStructListLength(
    __in            const DRM_XB_FORMAT_DESCRIPTION      *f_pFormat,
    __in_opt        const DRM_VOID                       *f_pvBaseObject,
    __in            const DRM_XB_ELEMENT_DESCRIPTION     *f_pElement,
    __inout_ecount( 1 )   DRM_DWORD                      *f_pcbLength )
{
    DRM_RESULT    dr       = DRM_SUCCESS;
    DRM_DWORD     cbLength = 0;
    DRM_SIZE_T    pdwCount;
    DRM_DWORD     cStructs = 0;

    /* Count is the offset indicated by wOffset from the start of the object */
    ChkArg( f_pvBaseObject != NULL );
    ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvBaseObject, f_pElement->xbSize.wOffset, &pdwCount ) );

    switch( f_pElement->xbSize.eSizeType )
    {
        case XB_SIZE_IS_WORD_IN_ANOTHER_STRUCT_MEMBER:
            cStructs = *( const DRM_WORD * )pdwCount;
            break;

        case XB_SIZE_IS_DWORD_IN_ANOTHER_STRUCT_MEMBER:
            cStructs = *( const DRM_DWORD * )pdwCount;
            break;

        default:
            DRMASSERT(FALSE);
            break;
    }

    DRMASSERT( f_pElement->xbSize.wMinSize == f_pElement->xbSize.wMaxSize );

    if( f_pElement->pElementDescription != NULL )
    {
        DRM_VOID  *pvStruct  = NULL;
        DRM_VOID **ppvStruct = NULL;
        DRM_DWORD  dwLength  = 0;
        DRM_DWORD  iStruct;

        ChkArg( f_pvBaseObject != NULL );

        /* Struct list elements are a pointer to an array of structures, so dereference the pointer here */
        ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvBaseObject, f_pElement->wOffsetInLwrrentStruct, ( DRM_SIZE_T * )&ppvStruct ) );
        pvStruct = *ppvStruct;

        for( iStruct = 0; iStruct < cStructs; iStruct++ )
        {
            ChkDR( _DRM_XB_GetStructLength( f_pFormat, pvStruct, f_pElement, &dwLength ) );
            ChkDR( DRM_DWordAddSame( &cbLength, dwLength ) );

            ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)pvStruct, f_pElement->xbSize.wMinSize, (DRM_SIZE_T*)&pvStruct) );
        }

    }
    else
    {
        ChkDR( DRM_DWordMult( cStructs, f_pElement->xbSize.wMinSize, &cbLength ) );
    }

    ChkDR( _DRM_XB_AddEstimatedAlignmentSize( f_pFormat, f_pElement->eElementType, &cbLength ) );

    *f_pcbLength = cbLength;

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   _DRM_XB_GetElementLength
**
** Synopsis :   Estimates the serialized size of the provided XBinary element.
**
** Arguments :
**              [f_pFormat     ] : The XBinary format structure.
**              [f_pvBaseObject] : A pointer to the object that contains the provided
**                                 element (f_pvObject).
**              [f_pvObject    ] : The target element object for which the size will be
**                                 callwlated.
**              [f_pElement    ] : The description for the target element (f_pvObject).
**              [f_pdwLength   ] : The serialized size of a single struct in the
**                                 structlist parameter f_pElement
**
** Returns:
**              Returns DRM_SUCCESS on success
**
******************************************************************************/
GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_RESULT DRM_CALL _DRM_XB_GetElementLength(
    __in            const DRM_XB_FORMAT_DESCRIPTION      *f_pFormat,
    __in_opt        const DRM_VOID                       *f_pvBaseObject,
    __in_opt        const DRM_VOID                       *f_pvObject,
    __in            const DRM_XB_ELEMENT_DESCRIPTION     *f_pElement,
    __inout_ecount( 1 )   DRM_DWORD                      *f_pcbLength )
{
    DRM_RESULT dr       = DRM_SUCCESS;
    DRM_DWORD  cbLength = 0;
    DRM_DWORD  dwResult = 0;
    DRM_DWORD  cbDelta  = 0;

    DRMASSERT( f_pFormat   != NULL );
    DRMASSERT( f_pElement  != NULL );
    DRMASSERT( f_pcbLength != NULL );

    switch( f_pElement->eElementType )
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
    case XB_ELEMENT_TYPE_GUID:
        cbDelta = sizeof( DRM_GUID );
        break;
    case XB_ELEMENT_TYPE_GUIDLIST:
        ChkArg( f_pvObject != NULL );
        ChkDR( DRM_DWordMult( ( ( DRM_XB_GUIDLIST *)f_pvObject )->cGUIDs, sizeof( DRM_GUID ), &dwResult ) );
        ChkDR( DRM_DWordAddSame( &cbLength, dwResult ) );
        cbDelta = sizeof( DRM_DWORD );
        break;
    case XB_ELEMENT_TYPE_WORDLIST:
        ChkArg( f_pvObject != NULL );
        ChkDR( DRM_DWordMult( ( ( DRM_XB_WORDLIST * )f_pvObject )->cWORDs,  sizeof( DRM_WORD ), &dwResult ) );
        ChkDR( DRM_DWordAddSame( &cbLength, dwResult ) );
        cbDelta = sizeof( DRM_DWORD );
        break;
    case XB_ELEMENT_TYPE_DWORDLIST:
        if( f_pvObject != NULL )
        {
            ChkDR( _DRM_XB_GetDwordListLength( f_pFormat, f_pvObject, f_pElement, &dwResult ) );
            cbDelta = dwResult;
        }
        break;
    case XB_ELEMENT_TYPE_QWORDLIST:
        ChkArg( f_pvObject != NULL );
        ChkDR( DRM_DWordMult( ( ( DRM_XB_QWORDLIST * )f_pvObject )->cQWORDs,  DRM_SIZEOFQWORD, &dwResult ) );
        ChkDR( DRM_DWordAddSame( &cbLength, dwResult ) );
        cbDelta = sizeof( DRM_DWORD );
        break;
    case XB_ELEMENT_TYPE_BYTEARRAY:
        ChkDR( _DRM_XB_GetByteArrayLength( f_pFormat, f_pvBaseObject, f_pvObject, f_pElement, &dwResult ) );
        cbDelta = dwResult;
        break;

    case XB_ELEMENT_TYPE_STRUCTLIST:
        ChkDR( _DRM_XB_GetStructListLength( f_pFormat, f_pvBaseObject, f_pElement, &dwResult ) );
        cbDelta = dwResult;
        break;

    case XB_ELEMENT_TYPE_UNKNOWN_OBJECT:
        /* This function is not called for UNKNOWN objects */
        DRMASSERT(FALSE);
        break;

    case XB_ELEMENT_TYPE_UNKNOWN_CONTAINER: __fallthrough;
    case XB_ELEMENT_TYPE_ELEMENT_COUNT:     __fallthrough;
    case XB_ELEMENT_TYPE_EMPTY:
        /* No data */
        break;

    default:
        DRMASSERT(FALSE);
        break;
    }

    ChkDR( DRM_DWordAddSame( &cbLength, cbDelta ) );

    *f_pcbLength = cbLength;

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   _DRM_XB_GetStructLength
**
** Synopsis :   Estimates the serialized size of a single struct in a structlist.
**
** Arguments :
**              [f_pFormat     ] : XBinary format structure
**              [f_pvObject    ] : The struct object.
**              [f_pStructList ] : The element description for the struct list.
**              [f_pdwLength   ] : The size of a single serialized structure
**                                indicated by f_pStructList.
**
** Returns:
**              Returns DRM_SUCCESS on success
**
******************************************************************************/
static DRM_RESULT DRM_CALL _DRM_XB_GetStructLength(
    __in       const DRM_XB_FORMAT_DESCRIPTION        *f_pFormat,
    __in       const DRM_VOID                         *f_pvObject,
    __in       const DRM_XB_ELEMENT_DESCRIPTION       *f_pStructList,
    __out            DRM_DWORD                        *f_pdwLength )

{
    DRM_RESULT                        dr       = DRM_SUCCESS;
    DRM_DWORD                         cbStruct = 0;
    const DRM_XB_ELEMENT_DESCRIPTION *pElems   = NULL;
    DRM_DWORD                         idx;

    DRMASSERT( f_pFormat     != NULL );
    DRMASSERT( f_pStructList != NULL );
    DRMASSERT( f_pdwLength   != NULL );

    pElems = ( const DRM_XB_ELEMENT_DESCRIPTION * )f_pStructList->pElementDescription;

    for( idx = 0; idx < f_pStructList->cElementDescription; idx++ )
    {
        DRM_DWORD cbElem;
        DRM_VOID  *pvElement = NULL;

        if( f_pvObject != NULL )
        {
            ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject, pElems[idx].wOffsetInLwrrentStruct, ( DRM_SIZE_T * )&pvElement ) );
        }

        ChkDR( _DRM_XB_GetElementLength( f_pFormat, f_pvObject, pvElement, &pElems[idx], &cbElem ) );
        ChkDR( DRM_DWordAddSame( &cbStruct, cbElem ) );
    }

    *f_pdwLength = cbStruct;

ErrorExit:
    return dr;
}


/******************************************************************************
**
** Function :   _DRM_XB_GetObjectLength
**
** Synopsis :   Estimates the length of an object. For a container, it only
**              returns the length of the base object
**
** Arguments :
**              [f_wType    ] : Object type
**              [f_pvObject ] : Actual object passed as void (optional)
**              [f_pFormat  ] : The XBinary format description.
**              [f_pdwLength] : The callwlated length of the XBinary object.
**
** Returns:
**              Returns the DRM_SUCCESS on success, otherwise an errror code.
**
******************************************************************************/
static DRM_RESULT DRM_CALL _DRM_XB_GetObjectLength(
    __in             DRM_WORD                          f_wType,
    __in_opt const   DRM_VOID                         *f_pvObject,
    __in     const   DRM_XB_FORMAT_DESCRIPTION        *f_pFormat,
    __out            DRM_DWORD                        *f_pdwLength )
{
    DRM_RESULT                        dr       = DRM_SUCCESS;
    DRM_DWORD                         cbLength = XB_BASE_OBJECT_LENGTH;
    const DRM_XB_ENTRY_DESCRIPTION   *pEntry   = NULL;
    const DRM_XB_ELEMENT_DESCRIPTION *pElement = NULL;
    DRM_DWORD                         iElement = 0;

    DRMASSERT( f_pFormat   != NULL );
    DRMASSERT( f_pdwLength != NULL );
    *f_pdwLength = 0;

    if( !_XB_IsKnownObjectType(f_pFormat, f_wType) )
    {
        goto ErrorExit;
    }

    pEntry = &(f_pFormat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pFormat, f_wType)]);
    if( pEntry->wFlags & XB_FLAGS_CONTAINER )
    {
        /*
        **  For containers we just return the base object length
        */
        *f_pdwLength = cbLength;
        goto ErrorExit;
    }

    /*
    **  Loop through each of the elements in the entry and sum up the length
    */
    for( iElement = 0; iElement < pEntry->cContainerOrObjectDescriptions; iElement++ )
    {
        const DRM_XB_ELEMENT_DESCRIPTION *pElementArray = (const DRM_XB_ELEMENT_DESCRIPTION*)pEntry->pContainerOrObjectDescriptions;
        DRM_VOID *pobject = NULL;
        DRM_DWORD cbElemLen = 0;

        pElement = &(pElementArray[iElement]);
        if( f_pvObject != NULL )
        {
            ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject, pElement->wOffsetInLwrrentStruct, (DRM_SIZE_T*)&pobject ) );
        }

        ChkDR( _DRM_XB_GetElementLength( f_pFormat, f_pvObject, pobject, pElement, &cbElemLen ) );
        ChkDR( DRM_DWordAddSame( &cbLength, cbElemLen ) );
    }

    *f_pdwLength = cbLength;

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   _DRM_XB_Serialize_UnknownBaseObject
**
** Synopsis :   Serializes the base object information (type, flags, size).
**              This information is seriailzed prior to EVERY object and
**              container.
**
** Arguments :
**      [f_wType   ] : The object/container type.
**      [f_wFlags  ] : The flags associated with the object/container type.
**      [f_dwLength] : The size the object/container (including the object
**                     base size).
**      [f_pbBuffer] : The serialization buffer.
**      [f_piBuffer] : The current index into the serialization buffer.
**
** Returns :
**      DRM_SUCCESS
**          Success
**      DRM_E_ILWALIDARG
**          One of the arguments was NULL
**      DRM_E_XB_UNKNOWN_ELEMENT_TYPE
**          A parsing error oclwrred
**      DRM_E_BUFFERTOOSMALL
**          The serialization buffer was not large enough
**
** Notes :  Prototyped in drmxbbuilder.h
******************************************************************************/
GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_VOID DRM_CALL _DRM_XB_Serialize_UnknownBaseObject(
    __in                                                   DRM_WORD    f_wType,
    __in                                                   DRM_WORD    f_wFlags,
    __in                                                   DRM_DWORD   f_dwLength,
    __inout_bcount( XB_BASE_OBJECT_LENGTH + *f_piBuffer )  DRM_BYTE   *f_pbBuffer,
    __inout_ecount( 1 )                                    DRM_DWORD  *f_piBuffer )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_DWORD dwIndex = *f_piBuffer;

    DRMASSERT( f_pbBuffer != NULL );
    DRMASSERT( f_piBuffer != NULL );

    WORD_TO_NETWORKBYTES(  f_pbBuffer, dwIndex, f_wFlags );
    ChkDR( DRM_DWordAddSame( &dwIndex, sizeof( DRM_WORD ) ) );

    WORD_TO_NETWORKBYTES(  f_pbBuffer, dwIndex, f_wType );
    ChkDR( DRM_DWordAddSame( &dwIndex, sizeof( DRM_WORD ) ) );

    DWORD_TO_NETWORKBYTES( f_pbBuffer, dwIndex, f_dwLength );
    ChkDR( DRM_DWordAddSame( &dwIndex, sizeof( DRM_DWORD ) ) );

    *f_piBuffer = dwIndex;

ErrorExit:
    if( DRM_FAILED( dr ) )
    {
        *f_piBuffer = 0;
    }
}

/*
** Serialize the basic type, flags, and length
*/
static DRM_VOID DRM_CALL _DRM_XB_Serialize_BaseObject(
    __in                                             const DRM_XB_BUILDER_CONTEXT_INTERNAL *f_pcontextBuilder,
    __in                                                   DRM_WORD                         f_wType,
    __in                                                   DRM_DWORD                        f_dwLength,
    __inout_bcount( XB_BASE_OBJECT_LENGTH + *f_piBuffer )  DRM_BYTE                        *f_pbBuffer,
    __inout_ecount( 1 )                                    DRM_DWORD                       *f_piBuffer,
    __in                                             const DRM_XB_FORMAT_DESCRIPTION       *f_pFormat )
{
    if ( _XB_IsKnownObjectType(f_pFormat, f_wType) )
    {
        _DRM_XB_Serialize_UnknownBaseObject( f_wType,
                                             f_pFormat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pFormat, f_wType)].wFlags,
                                             f_dwLength,
                                             f_pbBuffer,
                                             f_piBuffer );
    }
    else
    {
        DRMASSERT(FALSE);
    }
}

/*
** A series of serialize functions follow to serialize each of the simple element types that compose an object
*/

/******************************************************************************
**
** Function :   _DRM_XB_Serialize_WORD
**
** Synopsis :   Serializes a DRM_XB_WORD object.
**
** Arguments :
**      [f_pvObject] : A pointer to the DRM_XB_WORD object to be serialized.
**      [f_pbBuffer] : The serialization buffer.
**      [f_cbBuffer] : The size of the serialization buffer.
**      [f_piBuffer] : The current index into the serialization buffer.
**
** Returns :
**      DRM_SUCCESS
**          Success
**      DRM_E_ILWALIDARG
**          One of the arguments was NULL
**      DRM_E_XB_UNKNOWN_ELEMENT_TYPE
**          A parsing error oclwrred
**      DRM_E_BUFFERTOOSMALL
**          The serialization buffer was not large enough
**
** Notes :  Prototyped in drmxbbuilder.h
******************************************************************************/
GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_RESULT DRM_CALL _DRM_XB_Serialize_WORD(
    __in_ecount( 1 )  const      DRM_VOID    *f_pvObject,
    __inout_bcount( f_cbBuffer ) DRM_BYTE    *f_pbBuffer,
    __in                         DRM_DWORD    f_cbBuffer,
    __inout_ecount( 1 )          DRM_DWORD   *f_piBuffer )
{
    DRM_RESULT      dr         = DRM_SUCCESS;
    const DRM_WORD *pword      = ( const DRM_WORD* )f_pvObject;
    DRM_DWORD       dwResult   = 0;

    ChkArg( f_pvObject != NULL );
    ChkArg( f_pbBuffer != NULL );
    ChkArg( f_piBuffer != NULL );

    ChkDR( DRM_DWordAdd( *f_piBuffer, sizeof( DRM_WORD ), &dwResult ) );
    ChkBOOL( f_cbBuffer >= dwResult,
             DRM_E_BUFFERTOOSMALL );

    WORD_TO_NETWORKBYTES( f_pbBuffer, *f_piBuffer, *pword );
    ChkDR( DRM_DWordAddSame( f_piBuffer, sizeof( DRM_WORD ) ) );

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   _DRM_XB_Serialize_DWORD
**
** Synopsis :   Serializes a DRM_XB_DWORD object.
**
** Arguments :
**      [f_pvObject] : A pointer to the DRM_XB_DWORD object to be serialized.
**      [f_pbBuffer] : The serialization buffer.
**      [f_cbBuffer] : The size of the serialization buffer.
**      [f_piBuffer] : The current index into the serialization buffer.
**
** Returns :
**      DRM_SUCCESS
**          Success
**      DRM_E_ILWALIDARG
**          One of the arguments was NULL
**      DRM_E_XB_UNKNOWN_ELEMENT_TYPE
**          A parsing error oclwrred
**      DRM_E_BUFFERTOOSMALL
**          The serialization buffer was not large enough
**
** Notes :  Prototyped in drmxbbuilder.h
******************************************************************************/
GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_RESULT DRM_CALL _DRM_XB_Serialize_DWORD(
    __in_ecount( 1 )  const      DRM_VOID    *f_pvObject,
    __inout_bcount( f_cbBuffer ) DRM_BYTE    *f_pbBuffer,
    __in                         DRM_DWORD    f_cbBuffer,
    __inout_ecount( 1 )          DRM_DWORD   *f_piBuffer )
{
    DRM_RESULT       dr       = DRM_SUCCESS;
    const DRM_DWORD *pdword   = ( const DRM_DWORD* )f_pvObject;
    DRM_DWORD        dwResult = 0;

    ChkArg( f_pvObject != NULL );
    ChkArg( f_pbBuffer != NULL );
    ChkArg( f_piBuffer != NULL );

    ChkDR( DRM_DWordAdd( *f_piBuffer, sizeof( DRM_DWORD ), &dwResult ) );
    ChkBOOL( f_cbBuffer >= dwResult,
             DRM_E_BUFFERTOOSMALL );

    DWORD_TO_NETWORKBYTES( f_pbBuffer, *f_piBuffer, *pdword );
    ChkDR( DRM_DWordAddSame( f_piBuffer, sizeof( DRM_DWORD ) ) );

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   _DRM_XB_Serialize_QWORD
**
** Synopsis :   Serializes a DRM_XB_QWORD object.
**
** Arguments :
**      [f_pvObject] : A pointer to the DRM_XB_QWORD object to be serialized.
**      [f_pbBuffer] : The serialization buffer.
**      [f_cbBuffer] : The size of the serialization buffer.
**      [f_piBuffer] : The current index into the serialization buffer.
**
** Returns :
**      DRM_SUCCESS
**          Success
**      DRM_E_ILWALIDARG
**          One of the arguments was NULL
**      DRM_E_XB_UNKNOWN_ELEMENT_TYPE
**          A parsing error oclwrred
**      DRM_E_BUFFERTOOSMALL
**          The serialization buffer was not large enough
**
** Notes :  Prototyped in drmxbbuilder.h
******************************************************************************/
GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_RESULT DRM_CALL _DRM_XB_Serialize_QWORD(
    __in_ecount( 1 )  const      DRM_VOID    *f_pvObject,
    __inout_bcount( f_cbBuffer ) DRM_BYTE    *f_pbBuffer,
    __in                         DRM_DWORD    f_cbBuffer,
    __inout_ecount( 1 )          DRM_DWORD   *f_piBuffer )
{
    DRM_RESULT        dr       = DRM_SUCCESS;
    const DRM_UINT64 *pqword   = ( const DRM_UINT64* )f_pvObject;
    DRM_DWORD         dwResult = 0;

    ChkArg( f_pvObject != NULL );
    ChkArg( f_pbBuffer != NULL );
    ChkArg( f_piBuffer != NULL );

    ChkDR( DRM_DWordAdd( *f_piBuffer, DRM_SIZEOFQWORD, &dwResult ) );
    ChkBOOL( f_cbBuffer >= dwResult,
             DRM_E_BUFFERTOOSMALL );

    QWORD_TO_NETWORKBYTES( f_pbBuffer, *f_piBuffer, *pqword );
    ChkDR( DRM_DWordAddSame( f_piBuffer, DRM_SIZEOFQWORD ) );

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   _DRM_XB_Serialize_GUID
**
** Synopsis :   Serializes a DRM_GUID object.
**
** Arguments :
**      [f_pvObject] : A pointer to the DRM_GUID object to be serialized.
**      [f_pbBuffer] : The serialization buffer.
**      [f_cbBuffer] : The size of the serialization buffer.
**      [f_piBuffer] : The current index into the serialization buffer.
**
** Returns :
**      DRM_SUCCESS
**          Success
**      DRM_E_ILWALIDARG
**          One of the arguments was NULL
**      DRM_E_XB_UNKNOWN_ELEMENT_TYPE
**          A parsing error oclwrred
**      DRM_E_BUFFERTOOSMALL
**          The serialization buffer was not large enough
**
** Notes :  Prototyped in drmxbbuilder.h
******************************************************************************/
static DRM_RESULT DRM_CALL _DRM_XB_Serialize_GUID(
    __in_ecount( 1 )  const      DRM_VOID    *f_pvObject,
    __inout_bcount( f_cbBuffer ) DRM_BYTE    *f_pbBuffer,
    __in                         DRM_DWORD    f_cbBuffer,
    __inout_ecount( 1 )          DRM_DWORD   *f_piBuffer )
{
    DRM_RESULT       dr       = DRM_SUCCESS;
    const DRM_GUID  *pguid    = ( const DRM_GUID * )f_pvObject;
    DRM_DWORD        dwResult = 0;

    ChkArg( f_pvObject != NULL );
    ChkArg( f_pbBuffer != NULL );
    ChkArg( f_piBuffer != NULL );

    ChkDR( DRM_DWordAdd( *f_piBuffer, sizeof( DRM_GUID ), &dwResult ) );
    ChkBOOL( f_cbBuffer >= dwResult,
             DRM_E_BUFFERTOOSMALL );

    DRM_BYT_CopyBytes( f_pbBuffer,
                       *f_piBuffer,
                       pguid,
                       0,
                       sizeof( DRM_GUID ) );
    ChkDR( DRM_DWordAddSame( f_piBuffer, sizeof( DRM_GUID ) ) );

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   _DRM_XB_Serialize_GUIDLIST
**
** Synopsis :   Serializes a DRM_XB_GUIDLIST object.
**
** Arguments :
**      [f_pvObject] : A pointer to the DRM_XB_GUIDLIST object to be serialized.
**      [f_pxbSize ] : The element metadata for the GUID list which indicates
**                     how the object is to be serialized.
**      [f_pbBuffer] : The serialization buffer.
**      [f_cbBuffer] : The size of the serialization buffer.
**      [f_piBuffer] : The current index into the serialization buffer.
**
** Returns :
**      DRM_SUCCESS
**          Success
**      DRM_E_ILWALIDARG
**          One of the arguments was NULL
**      DRM_E_XB_UNKNOWN_ELEMENT_TYPE
**          A parsing error oclwrred
**      DRM_E_BUFFERTOOSMALL
**          The serialization buffer was not large enough
**
** Notes :  Prototyped in drmxbbuilder.h
******************************************************************************/
static DRM_RESULT DRM_CALL _DRM_XB_Serialize_GUIDLIST(
    __in_ecount( 1 )       const DRM_VOID    *f_pvObject,
    __in_ecount( 1 )       const DRM_XB_SIZE *f_pxbSize,
    __inout_bcount( f_cbBuffer ) DRM_BYTE    *f_pbBuffer,
    __in                         DRM_DWORD    f_cbBuffer,
    __inout_ecount( 1 )          DRM_DWORD   *f_piBuffer )
{
    DRM_RESULT             dr        = DRM_SUCCESS;
    DRM_DWORD              iBuffer   = 0;
    const DRM_XB_GUIDLIST *pguidlist = ( const DRM_XB_GUIDLIST * )f_pvObject;
    DRM_DWORD              dwResult  = 0;
    DRM_DWORD              cbGUIDs;

    ChkArg( f_pvObject != NULL );
    ChkArg( f_pbBuffer != NULL );
    ChkArg( f_piBuffer != NULL );
    ChkArg( (pguidlist->pguidBuffer != NULL) || (pguidlist->cGUIDs == 0) );

    iBuffer = *f_piBuffer;

    ChkDR( DRM_DWordMult( sizeof( DRM_GUID ), pguidlist->cGUIDs, &dwResult ) );
    cbGUIDs = dwResult;
    ChkDR( DRM_DWordAddSame( &dwResult, iBuffer ) );
    ChkDR( DRM_DWordAddSame( &dwResult, sizeof( DRM_DWORD ) ) );

    ChkBOOL( f_cbBuffer >= dwResult, DRM_E_BUFFERTOOSMALL );

    DWORD_TO_NETWORKBYTES( f_pbBuffer, iBuffer, pguidlist->cGUIDs );
    ChkDR( DRM_DWordAddSame( &iBuffer, sizeof( DRM_DWORD ) ) );

    ChkBOOL( pguidlist->cGUIDs >= f_pxbSize->wMinSize, DRM_E_XB_OBJECT_OUT_OF_RANGE );
    ChkBOOL( (f_pxbSize->wMaxSize == 0) || (pguidlist->cGUIDs <= f_pxbSize->wMaxSize), DRM_E_XB_OBJECT_OUT_OF_RANGE );

    if ( cbGUIDs > 0 && pguidlist->pguidBuffer != NULL )
    {
        dwResult = pguidlist->iGuids;

        DRM_BYT_CopyBytes(
            f_pbBuffer,
            iBuffer,
            pguidlist->pguidBuffer,
            dwResult,
            cbGUIDs );
    }

    ChkDR( DRM_DWordAddSame( &iBuffer, cbGUIDs ) );
    *f_piBuffer = iBuffer;

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   _DRM_XB_Serialize_WORDLIST
**
** Synopsis :   Serializes a DRM_XB_WORDLIST object.
**
** Arguments :
**      [f_pvObject] : A pointer to the DRM_XB_WORDLIST object to be serialized.
**      [f_pxbSize ] : The element metadata for the WORD list which indicates
**                     how the object is to be serialized.
**      [f_pbBuffer] : The serialization buffer.
**      [f_cbBuffer] : The size of the serialization buffer.
**      [f_piBuffer] : The current index into the serialization buffer.
**
** Returns :
**      DRM_SUCCESS
**          Success
**      DRM_E_ILWALIDARG
**          One of the arguments was NULL
**      DRM_E_XB_UNKNOWN_ELEMENT_TYPE
**          A parsing error oclwrred
**      DRM_E_BUFFERTOOSMALL
**          The serialization buffer was not large enough
**
** Notes :  Prototyped in drmxbbuilder.h
******************************************************************************/
static DRM_RESULT DRM_CALL _DRM_XB_Serialize_WORDLIST(
    __in_ecount( 1 )       const DRM_VOID    *f_pvObject,
    __in_ecount( 1 )       const DRM_XB_SIZE *f_pxbSize,
    __inout_bcount( f_cbBuffer ) DRM_BYTE    *f_pbBuffer,
    __in                         DRM_DWORD    f_cbBuffer,
    __inout_ecount( 1 )          DRM_DWORD   *f_piBuffer )
{
    DRM_RESULT             dr        = DRM_SUCCESS;
    DRM_DWORD              iBuffer   = 0;
    DRM_DWORD              iWords    = 0;
    const DRM_XB_WORDLIST *pwordlist = ( const DRM_XB_WORDLIST * )f_pvObject;
    DRM_DWORD              dwResult  = 0;
    DRM_BYTE              *pbResult  = NULL;

    ChkArg( f_pvObject != NULL );
    ChkArg( f_pbBuffer != NULL );
    ChkArg( f_piBuffer != NULL );
    ChkArg( (pwordlist->pwordBuffer != NULL) || (pwordlist->cWORDs == 0) );

    iBuffer = *f_piBuffer;

    ChkDR( DRM_DWordMult( sizeof( DRM_WORD ), pwordlist->cWORDs, &dwResult ) );
    ChkDR( DRM_DWordAdd( dwResult, iBuffer, &dwResult ) );
    ChkDR( DRM_DWordAdd( dwResult, sizeof( DRM_DWORD ), &dwResult ) );

    ChkBOOL( f_cbBuffer >= dwResult, DRM_E_BUFFERTOOSMALL );

    DWORD_TO_NETWORKBYTES( f_pbBuffer, iBuffer, pwordlist->cWORDs );
    ChkDR( DRM_DWordAddSame( &iBuffer, sizeof( DRM_DWORD ) ) );

    ChkBOOL( pwordlist->cWORDs >= f_pxbSize->wMinSize, DRM_E_XB_OBJECT_OUT_OF_RANGE );
    ChkBOOL( (f_pxbSize->wMaxSize == 0) || (pwordlist->cWORDs <= f_pxbSize->wMaxSize), DRM_E_XB_OBJECT_OUT_OF_RANGE );

    dwResult = pwordlist->iWords;
    for( iWords = 0; iWords < pwordlist->cWORDs; iWords++ )
    {
        ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)pwordlist->pwordBuffer, dwResult, (DRM_SIZE_T*)&pbResult ) );

        ChkDR( _DRM_XB_Serialize_WORD( pbResult, f_pbBuffer, f_cbBuffer, &iBuffer ) );

        ChkDR( DRM_DWordAddSame( &dwResult, sizeof( DRM_WORD ) ) );
    }
    *f_piBuffer = iBuffer;

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   _DRM_XB_Serialize_DWORDLIST
**
** Synopsis :   Serializes a DRM_XB_DWORDLIST object.
**
** Arguments :
**      [f_pvObject] : A pointer to the DRM_XB_DWORDLIST object to be serialized.
**      [f_pxbSize ] : The element metadata for the DWORD list which indicates
**                     how the object is to be serialized.
**      [f_pbBuffer] : The serialization buffer.
**      [f_cbBuffer] : The size of the serialization buffer.
**      [f_piBuffer] : The current index into the serialization buffer.
**
** Returns :
**      DRM_SUCCESS
**          Success
**      DRM_E_ILWALIDARG
**          One of the arguments was NULL
**      DRM_E_XB_UNKNOWN_ELEMENT_TYPE
**          A parsing error oclwrred
**      DRM_E_BUFFERTOOSMALL
**          The serialization buffer was not large enough
**
** Notes :  Prototyped in drmxbbuilder.h
******************************************************************************/
static DRM_RESULT DRM_CALL _DRM_XB_Serialize_DWORDLIST(
    __in_ecount( 1 )       const DRM_VOID       *f_pvObject,
    __in_ecount( 1 )       const DRM_XB_SIZE    *f_pxbSize,
    __inout_bcount( f_cbBuffer ) DRM_BYTE       *f_pbBuffer,
    __in                         DRM_DWORD       f_cbBuffer,
    __inout_ecount( 1 )          DRM_DWORD      *f_piBuffer )
{
    DRM_RESULT              dr         = DRM_SUCCESS;
    DRM_DWORD               iBuffer    = 0;
    DRM_DWORD               iDWords    = 0;
    const DRM_XB_DWORDLIST *pdwordlist = ( const DRM_XB_DWORDLIST * )f_pvObject;
    DRM_DWORD               dwResult   = 0;
    const DRM_DWORD        *pdwSource  = NULL;

    ChkArg( f_pvObject != NULL );
    ChkArg( f_pxbSize  != NULL );
    ChkArg( f_pbBuffer != NULL );
    ChkArg( f_piBuffer != NULL );
    ChkArg( (pdwordlist->pdwordBuffer != NULL) || (pdwordlist->cDWORDs == 0) );

    iBuffer = *f_piBuffer;

    ChkDR( DRM_DWordMult( sizeof( DRM_DWORD ), pdwordlist->cDWORDs, &dwResult ) );
    ChkDR( DRM_DWordAddSame( &dwResult, iBuffer ) );
    ChkBOOL( f_cbBuffer >= dwResult, DRM_E_BUFFERTOOSMALL );

    switch( f_pxbSize->eSizeType )
    {
        case XB_SIZE_IS_RELATIVE_WORD:
            ChkDR( DRM_DWordAdd( iBuffer, sizeof( DRM_WORD ), &dwResult ) );
            ChkBOOL( f_cbBuffer >= dwResult, DRM_E_BUFFERTOOSMALL );
            ChkBOOL( pdwordlist->cDWORDs <= DRM_MAX_UNSIGNED_TYPE( DRM_WORD ), DRM_E_XB_OBJECT_OUT_OF_RANGE );
            WORD_TO_NETWORKBYTES( f_pbBuffer, iBuffer, ( DRM_WORD )pdwordlist->cDWORDs );
            iBuffer = dwResult;
            break;

        case XB_SIZE_IS_WORD_IN_ANOTHER_STRUCT_MEMBER: __fallthrough;
        case XB_SIZE_IS_DWORD_IN_ANOTHER_STRUCT_MEMBER:
            /* Do nothing: count has already been serialized */
            break;

        default:
            ChkDR( DRM_DWordAdd( iBuffer, sizeof( DRM_DWORD ), &dwResult ) );
            ChkBOOL( f_cbBuffer >= dwResult, DRM_E_BUFFERTOOSMALL );
            DWORD_TO_NETWORKBYTES( f_pbBuffer, iBuffer, pdwordlist->cDWORDs );
            iBuffer = dwResult;
            break;
    }

    ChkBOOL( pdwordlist->cDWORDs >= f_pxbSize->wMinSize, DRM_E_XB_OBJECT_OUT_OF_RANGE );
    ChkBOOL( (f_pxbSize->wMaxSize == 0) || (pdwordlist->cDWORDs <= f_pxbSize->wMaxSize), DRM_E_XB_OBJECT_OUT_OF_RANGE );

    pdwSource = XB_DWORD_LIST_TO_PDWORD( *pdwordlist );
    for( iDWords = 0; iDWords < pdwordlist->cDWORDs; iDWords++ )
    {
        ChkDR( _DRM_XB_Serialize_DWORD( &pdwSource[iDWords], f_pbBuffer, f_cbBuffer, &iBuffer ) );
    }

    *f_piBuffer = iBuffer;

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   _DRM_XB_Serialize_QWORDLIST
**
** Synopsis :   Serializes a DRM_XB_QWORDLIST object.
**
** Arguments :
**      [f_pvObject] : A pointer to the DRM_XB_QWORDLIST object to be serialized.
**      [f_pxbSize ] : The element metadata for the QWORD list which indicates
**                     how the object is to be serialized.
**      [f_pbBuffer] : The serialization buffer.
**      [f_cbBuffer] : The size of the serialization buffer.
**      [f_piBuffer] : The current index into the serialization buffer.
**
** Returns :
**      DRM_SUCCESS
**          Success
**      DRM_E_ILWALIDARG
**          One of the arguments was NULL
**      DRM_E_XB_UNKNOWN_ELEMENT_TYPE
**          A parsing error oclwrred
**      DRM_E_BUFFERTOOSMALL
**          The serialization buffer was not large enough
**
** Notes :  Prototyped in drmxbbuilder.h
******************************************************************************/
static DRM_RESULT DRM_CALL _DRM_XB_Serialize_QWORDLIST(
    __in_ecount( 1 )  const      DRM_VOID    *f_pvObject,
    __in_ecount( 1 )       const DRM_XB_SIZE *f_pxbSize,
    __inout_bcount( f_cbBuffer ) DRM_BYTE    *f_pbBuffer,
    __in                         DRM_DWORD    f_cbBuffer,
    __inout_ecount( 1 )          DRM_DWORD   *f_piBuffer )
{
    DRM_RESULT              dr         = DRM_SUCCESS;
    DRM_DWORD               iBuffer    = 0;
    DRM_DWORD               iQWords    = 0;
    const DRM_XB_QWORDLIST *pqwordlist = ( const DRM_XB_QWORDLIST * )f_pvObject;
    DRM_DWORD               dwResult   = 0;
    DRM_BYTE               *pbResult   = NULL;

    ChkArg( f_pvObject != NULL );
    ChkArg( f_pbBuffer != NULL );
    ChkArg( f_piBuffer != NULL );
    ChkArg( (pqwordlist->pqwordBuffer != NULL) || (pqwordlist->cQWORDs == 0) );

    iBuffer = *f_piBuffer;

    ChkDR( DRM_DWordMult( DRM_SIZEOFQWORD, pqwordlist->cQWORDs, &dwResult ) );
    ChkDR( DRM_DWordAdd( dwResult, iBuffer, &dwResult ) );
    ChkDR( DRM_DWordAdd( dwResult, sizeof( DRM_DWORD ), &dwResult ) );

    ChkBOOL( f_cbBuffer >= dwResult, DRM_E_BUFFERTOOSMALL );

    DWORD_TO_NETWORKBYTES( f_pbBuffer, iBuffer, pqwordlist->cQWORDs );
    ChkDR( DRM_DWordAdd( iBuffer, sizeof( DRM_DWORD ), &iBuffer ) );

    ChkBOOL( pqwordlist->cQWORDs >= f_pxbSize->wMinSize, DRM_E_XB_OBJECT_OUT_OF_RANGE );
    ChkBOOL( (f_pxbSize->wMaxSize == 0) || (pqwordlist->cQWORDs <= f_pxbSize->wMaxSize), DRM_E_XB_OBJECT_OUT_OF_RANGE );

    ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)pqwordlist->pqwordBuffer, pqwordlist->iQwords, (DRM_SIZE_T*)&pbResult ) );

    for( iQWords = 0; iQWords < pqwordlist->cQWORDs; iQWords++ )
    {
        ChkDR( _DRM_XB_Serialize_QWORD( pbResult, f_pbBuffer, f_cbBuffer, &iBuffer ) );

        ChkDR( DRM_DWordPtrAddSame( ( DRM_SIZE_T * )&pbResult, DRM_SIZEOFQWORD ) );
    }

    *f_piBuffer = iBuffer;

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   _DRM_XB_Serialize_BYTEARRAY
**
** Synopsis :   Serializes a DRM_XB_BYTEARRAY object
**
** Arguments :
**      [f_pvObject] : A pointer to the DRM_XB_BYTEARRAY object to be serialized.
**      [f_pxbSize ] : The element metadata for the byte array which indicates
**                     how the object is to be serialized.
**      [f_eAlign  ] : The alignment option for the byte array.
**      [f_pbBuffer] : The serialization buffer.
**      [f_cbBuffer] : The size of the serialization buffer.
**      [f_piBuffer] : The current index into the serialization buffer.
**
** Returns :
**      DRM_SUCCESS
**          Success
**      DRM_E_ILWALIDARG
**          One of the arguments was NULL
**      DRM_E_XB_UNKNOWN_ELEMENT_TYPE
**          A parsing error oclwrred
**      DRM_E_BUFFERTOOSMALL
**          The serialization buffer was not large enough
**
** Notes :  Prototyped in drmxbbuilder.h
******************************************************************************/
static DRM_RESULT DRM_CALL _DRM_XB_Serialize_BYTEARRAY(
    __in_ecount( 1 )  const      DRM_VOID              *f_pvObject,
    __in_ecount( 1 )  const      DRM_XB_SIZE           *f_pxbSize,
    __in                         DRM_XB_ALIGNMENT       f_eAlign,
    __inout_bcount( f_cbBuffer ) DRM_BYTE              *f_pbBuffer,
    __in                         DRM_DWORD              f_cbBuffer,
    __inout_ecount( 1 )          DRM_DWORD             *f_piBuffer )
{
    DRM_RESULT              dr              = DRM_SUCCESS;
    const DRM_XB_BYTEARRAY *pbytearray      = ( const DRM_XB_BYTEARRAY * )f_pvObject;
    DRM_DWORD               dwResult        = 0;
    DRM_DWORD               cbPadding       = 0;
    DRM_BOOL                fIsDword        = TRUE;
    DRM_BOOL                fAddSize        = TRUE;
    DRM_BOOL                fSizeIsBits     = FALSE;
    DRM_BOOL                fFixedSize      = FALSE;
    DRM_DWORD               ibAligned;
    DRM_DWORD               dwSize          = 0;

    ChkArg( f_pvObject != NULL );
    ChkArg( f_pbBuffer != NULL );
    ChkArg( f_piBuffer != NULL );
    ChkArg( f_pxbSize  != NULL );
    ChkArg( pbytearray->cbData == 0 || pbytearray->pbDataBuffer != NULL );

    ibAligned = *f_piBuffer;

    switch ( f_pxbSize->eSizeType )
    {
    case XB_SIZE_IS_RELATIVE_WORD:
        fIsDword = FALSE;
        break;

    case XB_SIZE_IS_RELATIVE_DWORD:
        /* Use default values, do nothing */
        break;

    case XB_SIZE_IS_ABSOLUTE:
        fAddSize   = FALSE;
        fFixedSize = TRUE;
        break;

    case XB_SIZE_IS_RELATIVE_DWORD_IN_BITS:
        fSizeIsBits = TRUE;
        break;

    case XB_SIZE_IS_WORD_IN_ANOTHER_STRUCT_MEMBER:      __fallthrough;
    case XB_SIZE_IS_REMAINING_DATA_IN_OBJECT:           __fallthrough;
    case XB_SIZE_IS_DWORD_IN_ANOTHER_STRUCT_MEMBER:     __fallthrough;
    case XB_SIZE_IS_WORD_IN_BITS_ANOTHER_STRUCT_MEMBER: __fallthrough;
        fAddSize = FALSE;
        /* Other booleans are not used if fAddSize is FALSE */
        break;

    case XB_SIZE_IS_DATA_IN_OBJECT_FOR_WEAKREF: __fallthrough;
    case XB_SIZE_IS_ELEMENT_COUNT:
        /* Not serialized */
        goto ErrorExit;

    default:
        DRMASSERT(FALSE);
        ChkDR( DRM_E_NOTIMPL );
        break;
    }

    dwSize = pbytearray->cbData;
    if( dwSize > 0 )
    {
        DRM_WORD wPadding = f_pxbSize->wPadding;

        if( fAddSize )
        {
            /* Move the aligned index to after the cb where alignment needs to occur */
            ChkDR( DRM_DWordAddSame( &ibAligned, fIsDword ? sizeof(DRM_DWORD) : sizeof(DRM_WORD) ) );
        }

        /* Add size element to the alignment offset so that we can align only the pb data */
        ChkDR( _DRM_XB_AlignData( f_eAlign, f_cbBuffer, &ibAligned, &cbPadding ) );
        ChkDR( DRM_DWordAddSame( &dwSize, cbPadding ) );

        /* Add per element padding as described by the xbSize field */
        if( wPadding != XB_PADDING_NONE )
        {
            DRM_WORD wPaddingAmount = ( DRM_WORD )(dwSize % wPadding);
            if( wPaddingAmount != 0 )
            {
                wPaddingAmount = ( DRM_WORD )(wPadding - wPaddingAmount);
                ChkDR( DRM_DWordAddSame( &dwSize, wPaddingAmount ) );
            }
        }
    }

    /* If required, add the byte array size field prior to the data */
    if( fAddSize )
    {
        DRM_DWORD dwSizeValue;

        if( fSizeIsBits )
        {
            ChkDR( DRM_DWordMult( dwSize, 8, &dwSizeValue ) );
        }
        else
        {
            dwSizeValue = dwSize;
        }

        if( fIsDword )
        {
            ChkDR( _DRM_XB_Serialize_DWORD( &dwSizeValue, f_pbBuffer, f_cbBuffer, f_piBuffer ) );
        }
        else
        {
            DRM_WORD wSizeValue;

            ChkDR( DRM_DWordToWord( dwSizeValue, &wSizeValue ) );
            ChkDR( _DRM_XB_Serialize_WORD( &wSizeValue, f_pbBuffer, f_cbBuffer, f_piBuffer ) );
        }
    }

    ChkDR( DRM_DWordAdd( *f_piBuffer, dwSize, &dwResult ) );
    ChkBOOL( f_cbBuffer >= dwResult, DRM_E_BUFFERTOOSMALL );

    if( fFixedSize )
    {
        DRMASSERT( f_pxbSize->wMinSize == f_pxbSize->wMaxSize );
        AssertChkArg( pbytearray->cbData == f_pxbSize->wMinSize );
    }
    else
    {
        ChkBOOL( pbytearray->cbData >= f_pxbSize->wMinSize, DRM_E_XB_OBJECT_OUT_OF_RANGE );
        ChkBOOL( (f_pxbSize->wMaxSize == 0) || (pbytearray->cbData <= f_pxbSize->wMaxSize), DRM_E_XB_OBJECT_OUT_OF_RANGE );
    }

    if ( pbytearray->cbData > 0 )
    {
        AssertChkArg( *f_piBuffer <= ibAligned );
        /* zero out what was aligned */
        ZEROMEM( f_pbBuffer + ibAligned - cbPadding, cbPadding );

        DRM_BYT_CopyBytes( f_pbBuffer,
                           ibAligned,
                           pbytearray->pbDataBuffer,
                           pbytearray->iData,
                           pbytearray->cbData );

    }
    /* Add empty buffer for fixed size data that is not initialized */
    else if( fFixedSize && f_pxbSize->wMinSize > 0 )
    {
        ChkDR( DRM_DWordAdd( ibAligned, f_pxbSize->wMinSize, &dwResult ) );
        ChkBOOL( f_cbBuffer >= dwResult, DRM_E_BUFFERTOOSMALL );

        ZEROMEM( f_pbBuffer + ibAligned, f_pxbSize->wMinSize );
    }

    DRMASSERT( dwResult >= *f_piBuffer );
    *f_piBuffer = dwResult;

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   _DRM_XB_Serialize_STRUCTLIST
**
** Synopsis :   Serializes an array of C-style structures
**
** Arguments :
**      [f_pcontextBuilder] : The XBinary builder context
**      [f_pElement       ] : The element descriptor for the struct list
**      [f_pvBaseObject   ] : The object containing the struct list.  The container
**                            ALWAYS contains the count field which determines
**                            how many structs are in the array.
**      [f_pbBuffer       ] : The serialization buffer.
**      [f_cbBuffer       ] : The size of the serialization buffer.
**      [f_piBuffer       ] : The current index into the serialization buffer.
**
** Returns :
**      DRM_SUCCESS
**          Success
**      DRM_E_ILWALIDARG
**          One of the arguments was NULL
**      DRM_E_XB_UNKNOWN_ELEMENT_TYPE
**          A parsing error oclwrred
**      DRM_E_BUFFERTOOSMALL
**          The serialization buffer was not large enough
**
** Notes :  Prototyped in drmxbbuilder.h
******************************************************************************/
static DRM_RESULT DRM_CALL _DRM_XB_Serialize_STRUCTLIST(
    __in_ecount( 1 )       const DRM_XB_BUILDER_CONTEXT_INTERNAL    *f_pcontextBuilder,
    __in_ecount( 1 )       const DRM_XB_ELEMENT_DESCRIPTION         *f_pElement,
    __in_ecount( 1 )       const DRM_VOID                           *f_pvBaseObject,
    __inout_bcount( f_cbBuffer ) DRM_BYTE                           *f_pbBuffer,
    __in                         DRM_DWORD                           f_cbBuffer,
    __inout_ecount( 1 )          DRM_DWORD                          *f_piBuffer )
{
    DRM_RESULT       dr        = DRM_SUCCESS;
    DRM_VOID       **ppvObject = NULL;
    DRM_DWORD        cStructs  = 0;
    DRM_SIZE_T       pdwCount;
    DRM_XB_ALIGNMENT eAlign    = XB_ALIGN_1_BYTE;

    ChkArg( f_pcontextBuilder != NULL );
    ChkArg( f_pElement        != NULL );
    ChkArg( f_pvBaseObject    != NULL );
    ChkArg( f_pbBuffer        != NULL );
    ChkArg( f_piBuffer        != NULL );

    if( f_pcontextBuilder->pformat != NULL && f_pcontextBuilder->pformat->pHeaderDescription != NULL )
    {
        eAlign = f_pcontextBuilder->pformat->pHeaderDescription->eAlign;
    }

    ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvBaseObject, f_pElement->wOffsetInLwrrentStruct, (DRM_SIZE_T*)&ppvObject ) );

    /* Count is at the location indicated by wOffset from the base object */
    ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvBaseObject, f_pElement->xbSize.wOffset, &pdwCount ) );

    switch( f_pElement->xbSize.eSizeType )
    {
        case XB_SIZE_IS_WORD_IN_ANOTHER_STRUCT_MEMBER:
            cStructs = *( const DRM_WORD * )pdwCount;
            break;

        case XB_SIZE_IS_DWORD_IN_ANOTHER_STRUCT_MEMBER:
            cStructs = *( const DRM_DWORD * )pdwCount;
            break;

        default:
            AssertLogicError();
            break;
    }

    DRMASSERT( f_pElement->xbSize.wMinSize == f_pElement->xbSize.wMaxSize );

    if( f_pElement->pElementDescription != NULL )
    {
        DRM_VOID                         *pvObject  = *ppvObject;
        const DRM_XB_ELEMENT_DESCRIPTION *pElements = ( const DRM_XB_ELEMENT_DESCRIPTION * )f_pElement->pElementDescription;
        DRM_DWORD                         cElements = f_pElement->cElementDescription;
        DRM_DWORD                         iStruct;

        for( iStruct = 0; iStruct < cStructs; iStruct++ )
        {
            ChkDR( _DRM_XB_Serialize_ElementList(
                f_pcontextBuilder,
                cElements,
                pElements,
                pvObject,
                f_pbBuffer,
                f_cbBuffer,
                f_piBuffer ) );

            ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)pvObject, f_pElement->xbSize.wMinSize, (DRM_SIZE_T*)&pvObject) );
        }
    }
    else
    {
        DRM_DWORD dwTotalSize = 0;
        DRM_DWORD ibEnd       = 0;
        DRM_DWORD ibBuffer    = *f_piBuffer;
        DRM_DWORD cbPadding   = 0;

        ChkDR( _DRM_XB_AlignData( eAlign, f_cbBuffer, &ibBuffer, &cbPadding ) );

        ChkDR( DRM_DWordMult( cStructs, f_pElement->xbSize.wMinSize, &dwTotalSize ) );
        ChkDR( DRM_DWordAdd( ibBuffer, dwTotalSize, &ibEnd ) );
        ChkBOOL( ibEnd <= f_cbBuffer, DRM_E_BUFFERTOOSMALL );

        /* Zero out padded data */
        ZEROMEM( f_pbBuffer + ( ibBuffer - cbPadding ), cbPadding );

        DRM_BYT_CopyBytes( f_pbBuffer,
                           ibBuffer,
                           ( DRM_BYTE * )(*ppvObject),
                           0,
                           dwTotalSize );

        *f_piBuffer = ibEnd;
    }

ErrorExit:
    return dr;
}


/******************************************************************************
**
** Function :   _DRM_XB_Serialize_UnknownObject
**
** Synopsis :   Serializes unknown object
**
** Arguments :
**      [f_pvObject] : Actual object returned as void
**      [f_eAlign  ] : The alignment value for which to serialize the unknown
**                     object.
**      [f_cbObject] : Size of serialized object
**      [f_pbBuffer] : Object Buffer
**      [f_piBuffer] : Index marking beginning of object in the buffer
**      [f_pvObject] : Actual object returned as void
**
** Returns :
**      DRM_SUCCESS
**          Success
**      DRM_E_ILWALIDARG
**          One of the arguments was NULL
**      DRM_E_ILWALID_LICENSE
**          A parsing error oclwrred
**
** Notes :  Prototyped in drmxbbuilder.h
******************************************************************************/
GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_Serialize_UnknownObject(
    __in_ecount( 1 )  const                    DRM_VOID            *f_pvObject,
    __in                                       DRM_XB_ALIGNMENT     f_eAlign,
    __inout_bcount( f_cbBuffer )               DRM_BYTE            *f_pbBuffer,
    __in                                       DRM_DWORD            f_cbBuffer,
    __inout_ecount( 1 )                        DRM_DWORD           *f_piBuffer )
{
    DRM_RESULT                 dr              = DRM_SUCCESS;
    const DRM_BYTE            *pbObject        = NULL;
    DRM_DWORD                  iBuffer         = 0;
    const DRM_XB_BUILDER_NODE* pChildNode      = NULL;
    DRM_DWORD                  dwBufferToCopy  = 0;
    DRM_DWORD                  ibEnd           = 0;
    DRM_DWORD                  cbPadding       = 0;

    ChkArg( f_pvObject != NULL );
    ChkArg( f_pbBuffer != NULL );
    ChkArg( f_piBuffer != NULL );

    iBuffer = *f_piBuffer;

    pChildNode = (const DRM_XB_BUILDER_NODE *)f_pvObject;

    if( pChildNode->cbLength < XB_BASE_OBJECT_LENGTH )
    {
        ChkDR( DRM_E_XB_ILWALID_OBJECT );
    }

    dwBufferToCopy = pChildNode->cbLength - XB_BASE_OBJECT_LENGTH;

    ChkDR( _DRM_XB_AlignData( f_eAlign, f_cbBuffer, &iBuffer, &cbPadding ) );
    ChkDR( DRM_DWordAdd( iBuffer, dwBufferToCopy, &ibEnd ) );
    ChkBOOL( (f_cbBuffer >= ibEnd ), DRM_E_BUFFERTOOSMALL );

    *f_piBuffer = iBuffer;

    pbObject = ( const DRM_BYTE * )pChildNode->pvObject;

    /* zero out padded buffer */
    ZEROMEM( f_pbBuffer + ( *f_piBuffer - cbPadding ), cbPadding );

    DRM_BYT_CopyBytes( f_pbBuffer,
                       *f_piBuffer,
                       pbObject,
                       0,
                       dwBufferToCopy );

    *f_piBuffer += dwBufferToCopy;

ErrorExit:
    return dr;
}

static DRM_RESULT DRM_CALL _DRM_XB_Serialize_ElementList(
    __in_ecount( 1 )              const DRM_XB_BUILDER_CONTEXT_INTERNAL    *f_pcontextBuilder,
    __in                                DRM_DWORD                           f_cElements,
    __in_ecount( f_cElements )    const DRM_XB_ELEMENT_DESCRIPTION         *f_pElements,
    __in_ecount( 1 )              const DRM_VOID                           *f_pvObject,
    __inout_bcount( f_cbBuffer )        DRM_BYTE                           *f_pbBuffer,
    __in                                DRM_DWORD                           f_cbBuffer,
    __inout_ecount( 1 )                 DRM_DWORD                          *f_piBuffer  )
{
    DRM_RESULT       dr        = DRM_SUCCESS;
    DRM_DWORD        iElement  = 0;
    DRM_XB_ALIGNMENT eAlign    = XB_ALIGN_1_BYTE;

    if( f_pcontextBuilder                              != NULL &&
        f_pcontextBuilder->pformat                     != NULL &&
        f_pcontextBuilder->pformat->pHeaderDescription != NULL )
    {
        eAlign = f_pcontextBuilder->pformat->pHeaderDescription->eAlign;
    }

    for( iElement = 0; iElement < f_cElements; iElement++ )
    {
        DRM_VOID *pobject = NULL;
        const DRM_XB_ELEMENT_DESCRIPTION *pElement = &f_pElements[iElement];

        ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject, pElement->wOffsetInLwrrentStruct, (DRM_SIZE_T*)&pobject ) );

        switch( pElement->eElementType )
        {
        case XB_ELEMENT_TYPE_WORD:          ChkDR( _DRM_XB_Serialize_WORD         ( pobject, f_pbBuffer, f_cbBuffer, f_piBuffer ) ); break;
        case XB_ELEMENT_TYPE_DWORD:         ChkDR( _DRM_XB_Serialize_DWORD        ( pobject, f_pbBuffer, f_cbBuffer, f_piBuffer ) ); break;
        case XB_ELEMENT_TYPE_QWORD:         ChkDR( _DRM_XB_Serialize_QWORD        ( pobject, f_pbBuffer, f_cbBuffer, f_piBuffer ) ); break;
        case XB_ELEMENT_TYPE_GUID:          ChkDR( _DRM_XB_Serialize_GUID         ( pobject, f_pbBuffer, f_cbBuffer, f_piBuffer ) ); break;
        case XB_ELEMENT_TYPE_GUIDLIST:      ChkDR( _DRM_XB_Serialize_GUIDLIST     ( pobject, &pElement->xbSize, f_pbBuffer, f_cbBuffer, f_piBuffer ) ); break;
        case XB_ELEMENT_TYPE_WORDLIST:      ChkDR( _DRM_XB_Serialize_WORDLIST     ( pobject, &pElement->xbSize, f_pbBuffer, f_cbBuffer, f_piBuffer ) ); break;
        case XB_ELEMENT_TYPE_DWORDLIST:     ChkDR( _DRM_XB_Serialize_DWORDLIST    ( pobject, &pElement->xbSize, f_pbBuffer, f_cbBuffer, f_piBuffer ) ); break;
        case XB_ELEMENT_TYPE_QWORDLIST:     ChkDR( _DRM_XB_Serialize_QWORDLIST    ( pobject, &pElement->xbSize, f_pbBuffer, f_cbBuffer, f_piBuffer ) ); break;
        case XB_ELEMENT_TYPE_UNKNOWN_OBJECT:
            ChkDR( _DRM_XB_Serialize_UnknownObject(
                pobject,
                eAlign,
                f_pbBuffer,
                f_cbBuffer,
                f_piBuffer ) );
            break;
        case XB_ELEMENT_TYPE_STRUCTLIST:
            ChkDR( _DRM_XB_Serialize_STRUCTLIST(
                f_pcontextBuilder,
                pElement,
                f_pvObject,
                f_pbBuffer,
                f_cbBuffer,
                f_piBuffer ) );
            break;
        case XB_ELEMENT_TYPE_BYTEARRAY:
            ChkDR( _DRM_XB_Serialize_BYTEARRAY(
                pobject,
                &pElement->xbSize,
                eAlign,
                f_pbBuffer,
                f_cbBuffer,
                f_piBuffer ) );
            break;
        case XB_ELEMENT_TYPE_EMPTY:
            /* No data */
            break;

        case XB_ELEMENT_TYPE_ILWALID: __fallthrough;
        default:
            DRMASSERT( !"Unsupported object type.  Most likely there is an error in the format description tables." );
            ChkDR( DRM_E_XB_UNKNOWN_ELEMENT_TYPE );
            break;
        }
    }

ErrorExit:
    return dr;
}

/*
**  Serialize a while object.  The object header has already been serialized which means
**  just loop through all elements in the object and serailize each one.
*/
static DRM_RESULT DRM_CALL _DRM_XB_Serialize_Object(
    __in_ecount( 1 )  const      DRM_XB_BUILDER_CONTEXT_INTERNAL    *f_pcontextBuilder,
    __in_ecount(1)         const DRM_XB_ENTRY_DESCRIPTION           *f_pEntry,
    __in_ecount( 1 )       const DRM_VOID                           *f_pvObject,
    __inout_bcount( f_cbBuffer ) DRM_BYTE                           *f_pbBuffer,
    __in                         DRM_DWORD                           f_cbBuffer,
    __inout_ecount( 1 )          DRM_DWORD                          *f_piBuffer )
{
    DRM_RESULT                        dr        = DRM_SUCCESS;
    const DRM_XB_ELEMENT_DESCRIPTION *pElements = NULL;
    DRM_DWORD                         cElements = 0;

    ChkArg( f_pEntry   != NULL );
    ChkArg( f_pvObject != NULL );
    ChkArg( f_pbBuffer != NULL );
    ChkArg( f_piBuffer != NULL );

    if( f_pEntry->wFlags & XB_FLAGS_DO_NOT_SERIALIZE )
    {
        goto ErrorExit;
    }

    pElements = (const DRM_XB_ELEMENT_DESCRIPTION*) f_pEntry->pContainerOrObjectDescriptions;
    cElements = f_pEntry->cContainerOrObjectDescriptions;

    ChkDR( _DRM_XB_Serialize_ElementList(
        f_pcontextBuilder,
        cElements,
        pElements,
        f_pvObject,
        f_pbBuffer,
        f_cbBuffer,
        f_piBuffer ) );

ErrorExit:
    return dr;
}

/*
** Serialize a conainer.  This function is relwrsize if the container has subcontainers that need serializing.
** The header for the container has already been serialized so loop through each entry and serialize as appropriate.
*/
GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_RESULT DRM_CALL _DRM_XB_Serialize_Container(
    __in_ecount( 1 )  const      DRM_XB_BUILDER_CONTEXT_INTERNAL   *f_pcontextBuilder,
    __in_ecount( 1 )  const      DRM_VOID                          *f_pvObject,
    __inout_bcount( f_cbBuffer ) DRM_BYTE                          *f_pbBuffer,
    __in                         DRM_DWORD                          f_cbBuffer,
    __inout_ecount( 1 )          DRM_DWORD                         *f_piBuffer )
{
    DRM_RESULT                      dr          = DRM_SUCCESS;
    const DRM_XB_BUILDER_LISTNODE  *pnodeChild  = ( const DRM_XB_BUILDER_LISTNODE * )f_pvObject;
    const DRM_XB_BUILDER_UNKNOWNOBJECT_LISTNODE* pUnkNodeChild = NULL;
    DRM_XB_FORMAT_DESCRIPTION *pFormat = NULL;

    ChkArg( f_pcontextBuilder != NULL );
    ChkArg( f_pvObject        != NULL );
    ChkArg( f_pbBuffer        != NULL );
    ChkArg( f_piBuffer        != NULL );

    pFormat = ( DRM_XB_FORMAT_DESCRIPTION * )f_pcontextBuilder->pformat;

    ChkArg( pFormat != NULL );

    /* Serialize all the child nodes */
    while ( pnodeChild != NULL )
    {
        DRM_DWORD dwResult = 0;

        ChkDR( DRM_DWordAdd( *f_piBuffer, XB_BASE_OBJECT_LENGTH, &dwResult ) );
        ChkBOOL( f_cbBuffer >= dwResult,
                 DRM_E_BUFFERTOOSMALL );

        if( _DRM_XB_IsNativeObject( pnodeChild->Node.wType, pFormat ) )
        {
            DRM_DWORD ibBuffer = *f_piBuffer;
            DRM_DWORD cbChild  = 0;

            /*
            ** Skip over the base object serialization (including the object size) until we
            ** serialize the object. The serialized size will not be known until after the
            ** object is serialized because alignment is determined based on the address and
            ** current offset.  Therefore, we will add the base object data at the appropriate
            ** offset after serializing the object.
            */
            ChkDR( DRM_DWordAddSame( f_piBuffer, XB_BASE_OBJECT_LENGTH ) );

            if ( pnodeChild->Node.pvObject != NULL )
            {
                const DRM_XB_ENTRY_DESCRIPTION *pEntry = &pFormat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(pFormat, pnodeChild->Node.wType)];

                if( pEntry->wFlags & XB_FLAGS_CONTAINER )
                {
                    /* Relwrse for the contained container */
                    ChkDR( _DRM_XB_Serialize_Container( f_pcontextBuilder, pnodeChild->Node.pvObject, f_pbBuffer, f_cbBuffer, f_piBuffer ) );
                }
                else
                {
                    ChkDR( _DRM_XB_Serialize_Object( f_pcontextBuilder,
                                                     pEntry,
                                                     pnodeChild->Node.pvObject,
                                                     f_pbBuffer,
                                                     f_cbBuffer,
                                                     f_piBuffer ) );
                }
            }

            /*
            ** Fill in the base object data now that we know the serialized size.
            */
            ChkDR( DRM_DWordSub( *f_piBuffer, ibBuffer, &cbChild ) );
            _DRM_XB_Serialize_BaseObject( f_pcontextBuilder,
                                          pnodeChild->Node.wType,
                                          cbChild,
                                          f_pbBuffer,
                                          &ibBuffer,
                                          pFormat );

        }
        else
        {
            DRM_DWORD ibBuffer = *f_piBuffer;
            DRM_DWORD cbChild  = 0;

            ChkBOOL( f_cbBuffer >= *f_piBuffer + pnodeChild->Node.cbLength,
                     DRM_E_BUFFERTOOSMALL );

            /*
            ** Skip over the base object serialization (including the object size) until we
            ** serialize the object. The serialized size will not be known until after the
            ** object is serialized because alignment is determined based on the address and
            ** current offset.  Therefore, we will add the base object data at the appropriate
            ** offset after serializing the object.
            */
            ChkDR( DRM_DWordAddSame( f_piBuffer, XB_BASE_OBJECT_LENGTH ) );

            pUnkNodeChild = DRM_REINTERPRET_CONST_CAST( const DRM_XB_BUILDER_UNKNOWNOBJECT_LISTNODE, pnodeChild );

            if( pUnkNodeChild->wFlags & XB_FLAGS_CONTAINER )
            {
                /* Call Serialize only if this container has any children */
                if ( pnodeChild->Node.pvObject != NULL )
                {
                    ChkDR( _DRM_XB_Serialize_Container( f_pcontextBuilder,
                                                        pnodeChild->Node.pvObject,
                                                        f_pbBuffer,
                                                        f_cbBuffer,
                                                        f_piBuffer ) );
                }
            }
            else
            {
                DRM_XB_ALIGNMENT eAlign = XB_ALIGN_1_BYTE;

                if( pFormat->pHeaderDescription != NULL )
                {
                    eAlign = f_pcontextBuilder->pformat->pHeaderDescription->eAlign;
                }

                ChkDR( _DRM_XB_Serialize_UnknownObject(
                    pnodeChild,
                    eAlign,
                    f_pbBuffer,
                    f_cbBuffer,
                    f_piBuffer ) );
            }

            /*
            ** Fill in the base object data now that we know the serialized size.
            */
            ChkDR( DRM_DWordSub( *f_piBuffer, ibBuffer, &cbChild ) );
            _DRM_XB_Serialize_UnknownBaseObject( pUnkNodeChild->listNode.Node.wType,
                                                 pUnkNodeChild->wFlags,
                                                 cbChild,
                                                 f_pbBuffer,
                                                 &ibBuffer );

        }

        pnodeChild = pnodeChild->pNext;
    }

ErrorExit:
    return dr;
}

/*
**  Exists to insert the new node in a sorted manner if the format dictates that there is an order to which the elements should appear.
**  Order is deteremined by DRM_XB_ENTRY_DESCRIPTION.wBuilderSortOrder
*/
static DRM_RESULT DRM_CALL _DRM_XB_InsertNodeInChildListSorted(
    __in_ecount( 1 )    const DRM_XB_BUILDER_CONTEXT_INTERNAL    *f_pcontextBuilder,
    __inout                   DRM_XB_BUILDER_NODE                *pnodeParent,
    __inout                   DRM_XB_BUILDER_LISTNODE            *plistnodeChild )
{
    DRM_XB_BUILDER_LISTNODE *pLwrr = ( DRM_XB_BUILDER_LISTNODE * )pnodeParent->pvObject;
    DRM_XB_BUILDER_LISTNODE *pPrev = NULL;

    if ( _DRM_XB_IsNativeObject( plistnodeChild->Node.wType, f_pcontextBuilder->pformat ) )
    {
        while( pLwrr != NULL
            && (    f_pcontextBuilder->pformat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pcontextBuilder->pformat, plistnodeChild->Node.wType)].wBuilderSortOrder
                 >  f_pcontextBuilder->pformat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pcontextBuilder->pformat, pLwrr->Node.wType)].wBuilderSortOrder ) )
        {
            pPrev = pLwrr;
            pLwrr = pLwrr->pNext;
        }
    }
    else
    {
        const DRM_XB_BUILDER_UNKNOWNOBJECT_LISTNODE *pUnk = DRM_REINTERPRET_CONST_CAST( const DRM_XB_BUILDER_UNKNOWNOBJECT_LISTNODE, plistnodeChild );

        /*
        ** The old XMR Builder placed new unknown containers at the end of the list, so in order to maintain
        ** strict compatibility with the OLD XMR builer/parser we will do the same here.
        */
        if( 0 != (pUnk->wFlags & XB_FLAGS_CONTAINER) )
        {
            while( pLwrr != NULL )
            {
                pPrev = pLwrr;
                pLwrr = pLwrr->pNext;
            }
        }
    }

    if( pLwrr == ( DRM_XB_BUILDER_LISTNODE * )pnodeParent->pvObject )
    {
        /* Just insert at the head of the list */
        plistnodeChild->pNext = ( DRM_XB_BUILDER_LISTNODE * )pnodeParent->pvObject;
        pnodeParent->pvObject = ( const DRM_VOID * )plistnodeChild;
    }
    else
    {
        /* Insert after pPrev */
        plistnodeChild->pNext = pPrev->pNext;
        pPrev->pNext = plistnodeChild;
    }

    return DRM_SUCCESS;
}

/******************************************************************************
**
** Function :   _DRM_XB_AddHierarchy
**
** Synopsis :   Adds all the ancestors of a node, and builds the correct
**              hierarchy
**
** Arguments :  [f_pcontextBuilder] : XBinary builder context
**              [f_plistnode      ] : Linked list node containing the node whose
**                                    ancestors are to be added to the builder tree
**
** Returns :    DRM_E_XB_OBJECT_ALREADY_EXISTS if the node already exists, or one
**
******************************************************************************/
GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_RESULT DRM_CALL _DRM_XB_AddHierarchy(
    __inout_ecount( 1 ) DRM_XB_BUILDER_CONTEXT_INTERNAL    *f_pcontextBuilder,
    __inout_ecount( 1 ) DRM_XB_BUILDER_LISTNODE            *f_plistnode )
{
    /* Note: Internal function: No need to check input */
    DRM_RESULT                 dr               = DRM_SUCCESS;
    DRM_WORD                   wParent          = 0;
    DRM_XB_BUILDER_LISTNODE   *plistnodeChild   = f_plistnode;

    wParent = _DRM_XB_GetParent( plistnodeChild, f_pcontextBuilder->pformat );
    ChkArg( _XB_IsKnownObjectType(f_pcontextBuilder->pformat, wParent) );

    /* This loop adds any ancestors not already included */
    while ( wParent != XB_OBJECT_TYPE_ILWALID )
    {
        DRM_XB_BUILDER_NODE *pnodeParent; /* Initialized by _DRM_XB_GetBuilderNode */
        ChkArg( _XB_IsKnownObjectType(f_pcontextBuilder->pformat, wParent) );

        pnodeParent = _DRM_XB_GetBuilderNode( f_pcontextBuilder, wParent );

        if ( pnodeParent == NULL )
        {
            DRM_XB_BUILDER_LISTNODE *plistnode = NULL;
            DRM_DWORD                cbChild   = plistnodeChild->Node.cbLength;

            /*
            **  Add object for this parent
            */
            ChkDR( DRM_STK_Alloc( f_pcontextBuilder->pcontextStack,
                                  sizeof( DRM_XB_BUILDER_LISTNODE ),
                                  ( DRM_VOID * * ) &plistnode ) );

            plistnode->Node.wType = wParent;

            if( !_XB_IsKnownObjectType(f_pcontextBuilder->pformat, plistnodeChild->Node.wType ) )
            {
                /*
                ** Leave enough room for alignment
                */
                ChkDR( _DRM_XB_AddEstimatedAlignmentSize( f_pcontextBuilder->pformat, XB_ELEMENT_TYPE_UNKNOWN_OBJECT, &cbChild ) );
            }

            ChkDR( DRM_DWordAdd( XB_BASE_OBJECT_LENGTH, cbChild, &plistnode->Node.cbLength ) );
            plistnode->Node.pvObject  = ( const DRM_VOID * )plistnodeChild;
            plistnode->pNext          = NULL;
            f_pcontextBuilder->rgpObjectNodes[_XB_MapObjectTypeToEntryDescriptionIndex(f_pcontextBuilder->pformat, wParent)] = &plistnode->Node;
            plistnodeChild = plistnode;
            wParent = f_pcontextBuilder->pformat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pcontextBuilder->pformat, wParent)].wParent;
        }
        else
        {
            if ( !_DRM_XB_IsDuplicateAllowed( plistnodeChild, f_pcontextBuilder->pformat ) )
            {
                const DRM_XB_BUILDER_LISTNODE *plistnode = ( const DRM_XB_BUILDER_LISTNODE * )pnodeParent->pvObject;
                /*
                **  Check whether a duplicate exists
                */
                while( plistnode != NULL )
                {
                    if ( plistnode->Node.wType == plistnodeChild->Node.wType )
                    {
                        ChkDR( DRM_E_XB_OBJECT_ALREADY_EXISTS );
                    }
                    plistnode = plistnode->pNext;
                }
            }

            ChkDR( _DRM_XB_InsertNodeInChildListSorted( f_pcontextBuilder, pnodeParent, plistnodeChild ) );
            ChkDR( DRM_DWordAddSame( &pnodeParent->cbLength, plistnodeChild->Node.cbLength ) );
            break;
        }
    }

    /*
    **  Now update length of all ancestors of the current wParent
    */
    while ( wParent != XB_OBJECT_TYPE_ILWALID
         && f_pcontextBuilder->pformat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pcontextBuilder->pformat, wParent)].wParent != XB_OBJECT_TYPE_ILWALID )
    {
        wParent = f_pcontextBuilder->pformat->pEntryDescriptions[_XB_MapObjectTypeToEntryDescriptionIndex(f_pcontextBuilder->pformat, wParent)].wParent;

        ChkDR( DRM_DWordAddSame(
            &f_pcontextBuilder->rgpObjectNodes[_XB_MapObjectTypeToEntryDescriptionIndex(f_pcontextBuilder->pformat, wParent)]->cbLength,
             plistnodeChild->Node.cbLength ) );
    }

ErrorExit:
    return dr;
}


static DRM_RESULT DRM_CALL _DRM_XB_AddUnknownObjects(
    __in_ecount( 1 )          DRM_XB_BUILDER_CONTEXT                    *f_pcontextBuilder,
    __in_ecount( 1 )    const DRM_XB_UNKNOWN_OBJECT                     *f_pUnkObject,
    __in                      DRM_WORD                                   f_wParent )
{
    DRM_RESULT                   dr   = DRM_SUCCESS;
    const DRM_XB_UNKNOWN_OBJECT *pUnk = f_pUnkObject;

    while( pUnk != NULL )
    {
        if( pUnk->fValid )
        {
            const DRM_BYTE *pbData = pUnk->cbData == 0 ? NULL : pUnk->pbBuffer + pUnk->ibData;

            ChkDR( DRM_XB_AddUnknownObject( f_pcontextBuilder, pUnk->wType, TRUE, f_wParent, pUnk->wFlags, pUnk->cbData, pbData ) );
        }

        pUnk = pUnk->pNext;
    }

ErrorExit:
    return dr;
}

static DRM_RESULT DRM_CALL _DRM_XB_AddUnknownContainer(
    __in_ecount( 1 )          DRM_XB_BUILDER_CONTEXT                    *f_pcontextBuilder,
    __in_ecount( 1 )    const DRM_XB_UNKNOWN_CONTAINER                  *f_pUnkCont,
    __in                      DRM_WORD                                   f_wParent )
{
    DRM_RESULT                      dr       = DRM_SUCCESS;
    const DRM_XB_UNKNOWN_CONTAINER *pLwrCont = f_pUnkCont;

    while( pLwrCont != NULL )
    {
        if( pLwrCont->fValid )
        {
            ChkDR( DRM_XB_AddUnknownObject( f_pcontextBuilder, pLwrCont->wType, TRUE, f_wParent, pLwrCont->wFlags, 0, NULL ) );
            ChkDR( _DRM_XB_AddUnknownObjects( f_pcontextBuilder, ( const DRM_XB_UNKNOWN_OBJECT * )pLwrCont->pObject, pLwrCont->wType ) );
            ChkDR( _DRM_XB_AddUnknownContainer( f_pcontextBuilder, pLwrCont->pUnkChildcontainer, pLwrCont->wType ) );
        }

        pLwrCont = pLwrCont->pNext;
    }

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   _DRM_XB_AddContainer
**
** Synopsis :   Adds a container element and it's hierarchy to the builder node
**              list.
**
** Arguments :
**      [f_pcontextBuilder] : The XBinary builder context.
**      [f_wObjectType    ] : The type of the container object.
**      [f_pvObject       ] : The container object.
**
** Returns :
**      DRM_SUCCESS
**          Success
**      DRM_E_ILWALIDARG
**          One of the arguments was NULL
**      DRM_E_ILWALID_LICENSE
**          A parsing error oclwrred
**
** Notes :  Prototyped in drmxbbuilder.h
******************************************************************************/
static DRM_RESULT DRM_CALL _DRM_XB_AddContainer(
    __inout_ecount( 1 )    DRM_XB_BUILDER_CONTEXT    *f_pcontextBuilder,
    __in                   DRM_WORD                   f_wObjectType,
    __in_ecount( 1 ) const DRM_VOID                  *f_pvObject )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_DWORD iEntry;
    DRM_XB_BUILDER_CONTEXT_INTERNAL *pcontextBuilder =
                        DRM_REINTERPRET_CAST( DRM_XB_BUILDER_CONTEXT_INTERNAL, f_pcontextBuilder );

    for( iEntry = 0; iEntry < pcontextBuilder->pformat->cEntryDescriptions; iEntry++ )
    {
        const DRM_XB_ENTRY_DESCRIPTION *pContainerEntry = pcontextBuilder->pformat->pEntryDescriptions + iEntry;

        if( pContainerEntry->wParent == f_wObjectType && pContainerEntry->wType != XB_OBJECT_TYPE_ROOT )
        {
            if( pContainerEntry->wType == XB_OBJECT_TYPE_UNKNOWN )
            {
                DRM_XB_UNKNOWN_OBJECT **ppUnk = NULL;
                ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject, pContainerEntry->wOffsetInLwrrentStruct, (DRM_SIZE_T*)&ppUnk ) );

                if( ppUnk != NULL && *ppUnk != NULL )
                {
                    if( (*ppUnk)->fValid )
                    {
                        ChkDR( _DRM_XB_AddUnknownObjects( f_pcontextBuilder, *ppUnk, f_wObjectType ) );
                    }
                }
            }
            else if( pContainerEntry->wType == XB_OBJECT_TYPE_UNKNOWN_CONTAINER )
            {
                DRM_XB_UNKNOWN_CONTAINER *pUnkContainer = NULL;
                ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject, pContainerEntry->wOffsetInLwrrentStruct, (DRM_SIZE_T*)&pUnkContainer ) );

                if( pUnkContainer->fValid )
                {
                    ChkDR( _DRM_XB_AddUnknownContainer( f_pcontextBuilder, pUnkContainer, f_wObjectType ) );
                }
            }
            else if( pContainerEntry->wType   != XB_OBJECT_TYPE_ROOT )
            {
                /* This is an object that is a direct child of the global header.
                ** Add the entry to the builder
                */
                if( pContainerEntry->fDuplicateAllowed )
                {
                    /* Given there is a linked list here we have to do an extra deref */
                    const DRM_XB_BASELIST *pObject   = NULL;
                    DRM_VOID             **pvResult  = NULL;

                    ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject, pContainerEntry->wOffsetInLwrrentStruct, (DRM_SIZE_T*)&pvResult ) );
                    pObject = (const DRM_XB_BASELIST*)*pvResult;

                    while( pObject != NULL && pObject->fValid )
                    {
                        ChkDR( DRM_XB_AddEntry( f_pcontextBuilder, pContainerEntry->wType, pObject ) );
                        pObject = pObject->pNext;
                    }
                }
                else
                {
                    const DRM_XB_EMPTY *pObject = NULL;

                    ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pvObject, pContainerEntry->wOffsetInLwrrentStruct, (DRM_SIZE_T*)&pObject) );

                    if( pObject->fValid )
                    {
                        ChkDR( DRM_XB_AddEntry( f_pcontextBuilder, pContainerEntry->wType, pObject ) );
                    }
                }
            }
        }
    }

    /*
    ** If the mapped node for the container being added is NULL it means that no children for the container were added.
    ** Previously this was OK because an empty container was not needed and if children were eventually added the container
    ** would be added with the _DRM_XB_AddHierarchy function.  However, the XMR builder allows empty containers to indicate behavior
    ** (allowing playback/copy rights).  Therefore, we need to add the container node and its hierarchy even if it is lwrrently
    ** empty.
    */
    if( pcontextBuilder->rgpObjectNodes[_XB_MapObjectTypeToEntryDescriptionIndex(pcontextBuilder->pformat, f_wObjectType)] == NULL )
    {
        DRM_XB_BUILDER_LISTNODE *plistnode = NULL;
        ChkDR( DRM_STK_Alloc( pcontextBuilder->pcontextStack,
                              sizeof( DRM_XB_BUILDER_LISTNODE ),
                              ( DRM_VOID ** )&plistnode ) );
        plistnode->Node.pvObject = NULL;
        plistnode->Node.cbLength = XB_BASE_OBJECT_LENGTH;
        plistnode->Node.wType    = f_wObjectType;
        plistnode->pNext         = NULL;
        pcontextBuilder->rgpObjectNodes[_XB_MapObjectTypeToEntryDescriptionIndex(pcontextBuilder->pformat, f_wObjectType)] = &plistnode->Node;
        ChkDR( _DRM_XB_AddHierarchy( pcontextBuilder, plistnode ) );
    }

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   DRM_XB_AddEntry
**
** Synopsis :   Adds any object to the builder tree. This API can be used to add
**              either a leaf object, or a container structure
**
** Arguments :  [f_pcontextBuilder] : The XBinary builder context.
**              [f_wObjectType    ] : The object/container type.
**              [f_pvObject       ] : The object or container.
**
** Returns :    Unless a duplicate is permissible for this type, adding a
**              duplicate object will cause an error. If a container has been
**              explicitly added and then one of it's child nodes is added
**              separately, it is treated as a duplicate as well
**
** Notes :      The caller can add an empty container to a license.
**              The caller MUST make sure that no part of the object is freed,
**              or goes out of scope before the license is built. This is
**              because the internal builder context refers to the original
**              object ( instead of making a copy for efficiency reasons )
**
******************************************************************************/
DRM_API DRM_RESULT DRM_CALL DRM_XB_AddEntry(
    __inout_ecount( 1 )    DRM_XB_BUILDER_CONTEXT    *f_pcontextBuilder,
    __in                   DRM_WORD                   f_wObjectType,
    __in_ecount( 1 ) const DRM_VOID                  *f_pvObject )
{
    DRM_RESULT                       dr              = DRM_SUCCESS;
    DRM_XB_BUILDER_LISTNODE         *plistnode       = NULL;
    DRM_XB_BUILDER_CONTEXT_INTERNAL *pcontextBuilder =
                        DRM_REINTERPRET_CAST( DRM_XB_BUILDER_CONTEXT_INTERNAL, f_pcontextBuilder );
    DRM_WORD                         wObjectIndex    = 0;

    ChkArg( pcontextBuilder != NULL );
    ChkArg( pcontextBuilder->pformat != NULL );

    ChkArg( _XB_IsKnownObjectType(pcontextBuilder->pformat, f_wObjectType) );
    ChkArg( f_pvObject != NULL );
    ChkArg( pcontextBuilder->rgpObjectNodes != NULL );

    wObjectIndex = _XB_MapObjectTypeToEntryDescriptionIndex(pcontextBuilder->pformat, f_wObjectType);

    if( f_wObjectType == XB_OBJECT_GLOBAL_HEADER ||
       (pcontextBuilder->pformat->pEntryDescriptions[wObjectIndex].wFlags & XB_FLAGS_CONTAINER) )
    {
        ChkDR( _DRM_XB_AddContainer( f_pcontextBuilder, f_wObjectType, f_pvObject ) );
    }
    else
    {
        ChkDR( DRM_STK_Alloc( pcontextBuilder->pcontextStack,
                              sizeof( DRM_XB_BUILDER_LISTNODE ),
                              ( DRM_VOID ** )&plistnode ) );
        ChkDR( _DRM_XB_GetObjectLength( f_wObjectType, f_pvObject, pcontextBuilder->pformat, &plistnode->Node.cbLength ) );
        plistnode->Node.pvObject = f_pvObject;
        plistnode->Node.wType    = f_wObjectType;
        plistnode->pNext         = NULL;
        pcontextBuilder->rgpObjectNodes[wObjectIndex] = &plistnode->Node;
        ChkDR( _DRM_XB_AddHierarchy( pcontextBuilder, plistnode ) );
    }

ErrorExit:
    return dr;
}


/******************************************************************************
**
** Function :   DRM_XB_AddUnknownHierarchy
**
** Synopsis :   Adds all the ancestors of a node, and builds the correct
**              hierarchy
**
** Arguments :  [f_pcontextBuilder] : The XBinary builder context.
**              [f_plistnode      ] : Linked list node containing the node whose
**                                    ancestors are to be added to the builder tree
**
** Returns :
**      DRM_SUCCESS
**          Success
**      DRM_E_ILWALIDARG
**          One of the parameters is invalid
**      DRM_E_XB_OBJECT_ALREADY_EXISTS
**          Node already exists
**      DRM_E_XB_OBJECT_NOT_FOUND
**          Failed to find the parent node
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_XB_AddUnknownHierarchy(
    __inout_ecount( 1 )  DRM_XB_BUILDER_CONTEXT_INTERNAL        *f_pcontextBuilder,
    __inout_ecount( 1 )  DRM_XB_BUILDER_UNKNOWNOBJECT_LISTNODE  *f_plistnode )
{
    /* Note: Internal function: No need to check input */
    DRM_RESULT                   dr          = DRM_SUCCESS;
    DRM_WORD                     wParent     = 0;
    DRM_XB_BUILDER_NODE         *pnodeParent = NULL;
    const DRM_XB_BUILDER_NODE   *pnode       = NULL;

    DRM_XB_BUILDER_UNKNOWNOBJECT_LISTNODE *plistnodeChild = f_plistnode;

    wParent =  f_plistnode->wParent;
    pnode   = &( f_plistnode->listNode.Node );

    pnodeParent = _DRM_XB_GetBuilderNode( f_pcontextBuilder, wParent );
    AssertChkArg( pnodeParent == NULL || ( pnode->wType != pnodeParent->wType ) );

    if ( _DRM_XB_IsNativeObject( wParent, f_pcontextBuilder->pformat ) )
    {
        /*
        ** All known objects are deferred to _DRM_XB_AddHierarchy
        */
        ChkDR( _DRM_XB_AddHierarchy( f_pcontextBuilder, &( plistnodeChild->listNode ) ) );
    }
    else
    {
        DRM_DWORD cbUnknownObject = 0;
        DRM_DWORD lwnknownParents = 1; /* This else block indicates the parent is an unknown container */

        /*
        ** We require that if the parent of the object is not a well known
        ** XB object, parent should be added before adding the node itself.
        ** Therefore, the builder node for the parent should be non-null
        */

        if( pnodeParent == NULL )
        {
            ChkDR( DRM_E_XB_OBJECT_NOTFOUND );
        }

        cbUnknownObject = f_plistnode->listNode.Node.cbLength;

        /*
        ** Leave enough room for alignment
        */
        ChkDR( _DRM_XB_AddEstimatedAlignmentSize( f_pcontextBuilder->pformat, XB_ELEMENT_TYPE_UNKNOWN_OBJECT, &cbUnknownObject ) );

        /*
        **  Insert this node into parent's child list
        */
        f_plistnode->listNode.pNext = ( DRM_XB_BUILDER_LISTNODE * )pnodeParent->pvObject;
        pnodeParent->pvObject = ( const DRM_VOID * )&( f_plistnode->listNode );
        ChkDR( DRM_DWordAddSame( &pnodeParent->cbLength, cbUnknownObject ) );

        /*
        **  Now update length of all ancestors of the current wParent
        */
        while ( wParent != XB_OBJECT_GLOBAL_HEADER && pnodeParent != NULL )
        {
            wParent = _DRM_XB_GetParent( DRM_REINTERPRET_CONST_CAST( const DRM_XB_BUILDER_LISTNODE, pnodeParent ), f_pcontextBuilder->pformat );
            pnodeParent = _DRM_XB_GetBuilderNode( f_pcontextBuilder, wParent );
            DRMASSERT( pnodeParent != NULL );
            if(pnodeParent != NULL )
            {
                ChkDR( DRM_DWordAddSame( &pnodeParent->cbLength, cbUnknownObject ) );
            }

            if( !_DRM_XB_IsNativeObject( wParent, f_pcontextBuilder->pformat ) )
            {
                lwnknownParents++;
                ChkBOOL( lwnknownParents <= DRM_XB_PARSER_MAX_UNKNOWN_CONTAINER_DEPTH, DRM_E_XB_MAX_UNKNOWN_CONTAINER_DEPTH );
            }
        }
    }

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   DRM_XB_AddUnknownObject
**
** Synopsis :   Adds unknown object to the builder tree. This API can be used to add
**              either a leaf object, or a container structure
**
** Arguments :  [f_pcontextBuilder] : The XBinary builder context.
**              [f_wObjectType    ] : The unknown object or container type.
**              [f_pvObject       ] : The unknown object orcontainer.
**
** Returns :    Unless a duplicate is permissible for this type, adding a
**              duplicate object will cause an error. If a container has been
**              explicitly added and then one of it's child nodes is added
**              separately, it is treated as a duplicate as well
**
** Notes :      The caller can add an empty container to a license.
**              The caller MUST make sure that no part of the object is freed,
**              or goes out of scope before the license is built. This is
**              because the internal builder context refers to the original
**              object ( instead of making a copy for efficiency reasons )
**
******************************************************************************/
DRM_API DRM_RESULT DRM_CALL DRM_XB_AddUnknownObject(
    __inout_ecount( 1 )                 DRM_XB_BUILDER_CONTEXT  *f_pcontextBuilder,
    __in                                DRM_WORD                 f_wObjectType,
    __in                                DRM_BOOL                 f_fDuplicateAllowed,
    __in                                DRM_WORD                 f_wParent,
    __in                                DRM_WORD                 f_wFlags,
    __in                                DRM_DWORD                f_cbObject,
    __in_bcount_opt( f_cbObject ) const DRM_BYTE                *f_pbObject )
{
    DRM_RESULT dr  = DRM_SUCCESS;
    DRM_XB_BUILDER_UNKNOWNOBJECT_LISTNODE *plistnode  = NULL;
    DRM_XB_BUILDER_CONTEXT_INTERNAL       *pcontextBuilder =
            DRM_REINTERPRET_CAST( DRM_XB_BUILDER_CONTEXT_INTERNAL, f_pcontextBuilder );

    ChkArg( pcontextBuilder != NULL );
    ChkArg( (f_pbObject == NULL) == (f_cbObject == 0) );

    /*
    ** Any object which is marked external parse can be added through this function,
    ** but certain objects which are not marked external parse can also be so added.
    ** Therefore, do NOT include this ChkArg or the latter will be blocked.
    ** ChkArg( f_wFlags & XB_FLAGS_ALLOW_EXTERNAL_PARSE );
    */

    ChkDR( DRM_STK_Alloc( pcontextBuilder->pcontextStack,
        sizeof( DRM_XB_BUILDER_UNKNOWNOBJECT_LISTNODE ),
        ( DRM_VOID ** )&plistnode ) );

    ZEROMEM( plistnode, sizeof( DRM_XB_BUILDER_UNKNOWNOBJECT_LISTNODE ) );

    plistnode->listNode.Node.wType      = f_wObjectType;
    plistnode->listNode.Node.cbLength   = XB_BASE_OBJECT_LENGTH + f_cbObject;
    plistnode->listNode.Node.pvObject   = f_pbObject;
    plistnode->wParent                  = f_wParent;
    plistnode->pNext                    = NULL;
    plistnode->fDuplicateAllowed        = f_fDuplicateAllowed;
    plistnode->wFlags                   = f_wFlags;

    /*
    ** Add it to linked list
    */
    plistnode->pNext = pcontextBuilder->pUnknownObjects;
    pcontextBuilder->pUnknownObjects = plistnode;

    ChkDR( DRM_XB_AddUnknownHierarchy ( pcontextBuilder, plistnode ) );

    plistnode = NULL;

ErrorExit:
    if ( pcontextBuilder != NULL && plistnode != NULL )
    {
        (DRM_VOID)DRM_STK_Free( pcontextBuilder->pcontextStack, ( DRM_VOID * )plistnode );
    }
    return dr;
}


/******************************************************************************
**
** Function :   DRM_XB_StartFormatFromStack
**
** Synopsis :   Builder API used to initiate a builder context
**
** Arguments :  [f_pStack         ] : The stack allocator context to allocate XBinary
**                                    structures during serialization.
**              [f_dwVersion      ] : The version to be applied to the serialized object.
**              [f_pcontextBuilder] : The XBinary builder context.
**              [f_pformat        ] : The XBinary format description of the object to be
**                                    serialized.
**
** Notes    :   Memory will not be freed from this stack. The caller should just
**              throw away this stack buffer after the license has been built.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_XB_StartFormatFromStack(
    __inout                     DRM_STACK_ALLOCATOR_CONTEXT *f_pStack,
    __in                        DRM_DWORD                    f_dwVersion,
    __inout_ecount( 1 )         DRM_XB_BUILDER_CONTEXT      *f_pcontextBuilder,
    __in                  const DRM_XB_FORMAT_DESCRIPTION   *f_pformat )
{
    DRM_RESULT                          dr                   = DRM_SUCCESS;
    DRM_XB_BUILDER_NODE                *pnodeOuterContainer  = NULL;
    DRM_XB_BUILDER_CONTEXT_INTERNAL    *pcontextBuilder      = DRM_REINTERPRET_CAST( DRM_XB_BUILDER_CONTEXT_INTERNAL, f_pcontextBuilder );
    DRM_DWORD                           dwResult             = 0;

    ChkArg( pcontextBuilder != NULL );
    ChkArg( f_pStack        != NULL );
    ChkArg( f_pformat       != NULL );

    pcontextBuilder->dwVersion       = f_dwVersion;
    pcontextBuilder->pcontextStack   = f_pStack;
    pcontextBuilder->pformat         = f_pformat;
    pcontextBuilder->pUnknownObjects = NULL;

    /*
    **  Add a table for list of containers (add on extra for extended header information)
    */
    ChkDR( DRM_DWordAdd( f_pformat->cEntryDescriptions, 1, &dwResult ) );
    ChkDR( DRM_DWordMult( sizeof( DRM_XB_BUILDER_NODE * ), dwResult, &dwResult ) );

    ChkDR( DRM_STK_Alloc( pcontextBuilder->pcontextStack,
                          dwResult,
                          ( DRM_VOID * * )&pcontextBuilder->rgpObjectNodes ) );
    ZEROMEM( pcontextBuilder->rgpObjectNodes,
             dwResult );

    /*
    **  Add the outer container object
    */
    ChkDR( DRM_STK_Alloc( pcontextBuilder->pcontextStack,
                          sizeof( DRM_XB_BUILDER_NODE ),
                          ( DRM_VOID * * )&pnodeOuterContainer ) );

    pnodeOuterContainer->wType     = XB_OBJECT_GLOBAL_HEADER;
    pnodeOuterContainer->cbLength  = 0;
    pnodeOuterContainer->pvObject  = NULL;
    pcontextBuilder->rgpObjectNodes[XB_OBJECT_GLOBAL_HEADER] = pnodeOuterContainer;

ErrorExit:
    return dr;
}


/******************************************************************************
**
** Function :   DRM_XB_StartFormat
**
** Synopsis :   Builder API used to initiate a builder context
**
** Arguments :  [f_pbStack        ] : The stack buffer used to allocate XBinary structures
**                                    during serialization.
**              [f_cbStack        ] : The size of the stack buffer.
**              [f_dwVersion      ] : The version to be applied to the serialized object.
**              [f_pcontextBuilder] : The XBinary builder context.
**              [f_pformat        ] : The XBinary format description of the object to be
**                                    serialized.
**
** Notes    :   Memory will not be freed from this stack. The caller should just
**              throw away this stack buffer after the license has been built.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_XB_StartFormat(
    __inout_bcount( f_cbStack ) DRM_BYTE                    *f_pbStack,
    __in                        DRM_DWORD                    f_cbStack,
    __in                        DRM_DWORD                    f_dwVersion,
    __inout_ecount( 1 )         DRM_XB_BUILDER_CONTEXT      *f_pcontextBuilder,
    __in                  const DRM_XB_FORMAT_DESCRIPTION   *f_pformat )
{
    DRM_RESULT                          dr                   = DRM_SUCCESS;
    DRM_XB_BUILDER_CONTEXT_INTERNAL    *pcontextBuilder      = DRM_REINTERPRET_CAST( DRM_XB_BUILDER_CONTEXT_INTERNAL, f_pcontextBuilder );

    ChkArg( f_pcontextBuilder != NULL );
    ChkArg( f_pbStack         != NULL );
    ChkArg( f_cbStack          > 0    );
    ChkArg( f_pformat         != NULL );

    ChkDR( DRM_STK_Init( &pcontextBuilder->oStack, f_pbStack, f_cbStack, TRUE ) );
    ChkDR( DRM_XB_StartFormatFromStack( &pcontextBuilder->oStack, f_dwVersion, f_pcontextBuilder, f_pformat ) );

ErrorExit:
    return dr;
}

/******************************************************************************
**
** Function :   DRM_XB_FinishFormat
**
** Synopsis :   Builder API used to either: callwlate the serialization buffer size
**              required; or to apply the serialization to the provided buffer.
**
** Arguments :  [f_pcontextBuilder] : The XBinary builder context.
**              [f_pbFormat       ] : The serialization buffer.  This parameter is optional.
**                                    If f_pbFormat is NULL, the serialization size will
**                                    be callwlated and returned via f_pcbFormat.
**              [f_pcbFormat      ] : On input, the size of the serialization buffer. On
**                                    output, the total size of the serialized data.
**
******************************************************************************/
DRM_API DRM_RESULT DRM_CALL DRM_XB_FinishFormat(
    __in_ecount( 1 )                                                              const DRM_XB_BUILDER_CONTEXT *f_pcontextBuilder,
    __inout_bcount_opt( *f_pcbFormat )                                                  DRM_BYTE               *f_pbFormat,
    __inout_ecount( 1 ) _Post_satisfies_( *f_pcbFormat <= _Old_( *f_pcbFormat ) )       DRM_DWORD              *f_pcbFormat )
{
    DRM_BYTE    *pbBuffer         = f_pbFormat;
    DRM_DWORD    cbFormat         = 0;
    DRM_DWORD    iFormat          = 0;
    DRM_RESULT   dr               = DRM_SUCCESS;
    DRM_WORD     iObject          = 0;
    DRM_DWORD    dwResult         = 0;
    DRM_DWORD    ibExtHeader      = 0;

    const DRM_XB_BUILDER_CONTEXT_INTERNAL *pcontextBuilder = DRM_REINTERPRET_CAST( const DRM_XB_BUILDER_CONTEXT_INTERNAL, ( DRM_XB_BUILDER_CONTEXT * )f_pcontextBuilder );

    ChkArg( pcontextBuilder  != NULL );
    ChkArg( f_pcbFormat     != NULL );
    ChkArg( pcontextBuilder->rgpObjectNodes != NULL );

    ChkBOOL( pcontextBuilder->rgpObjectNodes[ XB_OBJECT_GLOBAL_HEADER ] != NULL, DRM_E_XB_REQUIRED_OBJECT_MISSING );

    /*
    **  Ensure that required objects were added
    */
    for( iObject = 0;
         iObject < pcontextBuilder->pformat->cEntryDescriptions;
         iObject++ )
    {
        if(  pcontextBuilder->rgpObjectNodes[iObject] == NULL
         && !pcontextBuilder->pformat->pEntryDescriptions[iObject].fOptional )
        {
            ChkDR( DRM_E_XB_REQUIRED_OBJECT_MISSING );
        }
    }

    ChkDR( DRM_DWordAdd( XB_HEADER_LENGTH( pcontextBuilder->pformat->pHeaderDescription->eFormatIdLength ),
                         pcontextBuilder->rgpObjectNodes[XB_OBJECT_GLOBAL_HEADER]->cbLength,
                         &cbFormat ) );

    if ( *f_pcbFormat < cbFormat || f_pbFormat == NULL )
    {
        *f_pcbFormat = cbFormat;
        ChkDR( DRM_E_BUFFERTOOSMALL );
    }
    *f_pcbFormat = cbFormat;

    /* Serialize the Header */
    switch( pcontextBuilder->pformat->pHeaderDescription->eFormatIdLength )
    {
    case XB_FORMAT_ID_LENGTH_DWORD:
        DWORD_TO_NETWORKBYTES( pbBuffer, iFormat, DRM_UI64Low32( pcontextBuilder->pformat->pHeaderDescription->qwFormatIdentifier ) );
        DRMASSERT( iFormat == 0 );
        iFormat += XB_FORMAT_ID_LENGTH_DWORD;  /* iFormat is 0 before this */
        break;
    case XB_FORMAT_ID_LENGTH_QWORD:
        QWORD_TO_NETWORKBYTES( pbBuffer, iFormat, pcontextBuilder->pformat->pHeaderDescription->qwFormatIdentifier );
        DRMASSERT( iFormat == 0 );
        iFormat += XB_FORMAT_ID_LENGTH_QWORD; /* iFormat is 0 before this */
        break;
    default:
        ChkDR( DRM_E_XB_ILWALID_OBJECT );
        break;
    }

    /*
    ** Add the extended header (if neccessary)
    */
    if( pcontextBuilder->pformat->pHeaderDescription->pEntryDescription != NULL )
    {
        ChkBOOL( pcontextBuilder->rgpObjectNodes[pcontextBuilder->pformat->cEntryDescriptions] != NULL, DRM_E_XB_REQUIRED_OBJECT_MISSING );

        ibExtHeader = iFormat;

        ChkDR( _DRM_XB_Serialize_Object(
            pcontextBuilder,
            pcontextBuilder->pformat->pHeaderDescription->pEntryDescription,
            pcontextBuilder->rgpObjectNodes[pcontextBuilder->pformat->cEntryDescriptions]->pvObject,
            pbBuffer,
            *f_pcbFormat,
            &iFormat ) );
    }
    else
    {
        DWORD_TO_NETWORKBYTES( pbBuffer, iFormat, pcontextBuilder->pformat->pHeaderDescription->dwFormatVersion );
        /* Leave room for the final size to be updated after the rest of the serialization */
        iFormat += 2 * sizeof( DRM_DWORD );
        DRMASSERT(     iFormat == 2 * sizeof( DRM_DWORD ) + XB_FORMAT_ID_LENGTH_QWORD
                    || iFormat == 2 * sizeof( DRM_DWORD ) + XB_FORMAT_ID_LENGTH_DWORD );
    }


    /* Serialize the Outer Container */
    ChkDR( DRM_DWordAdd( XB_BASE_OBJECT_LENGTH, iFormat, &dwResult ) );
    ChkBOOL( dwResult <= *f_pcbFormat, DRM_E_BUFFERTOOSMALL );

    /*
    ** Only serialize the outer container if it is serializable. Some formats may only want to serialize header information.
    ** The BCERT builder only serializes the header information for CERT chain headers.
    */
    if( pcontextBuilder->pformat->pEntryDescriptions[0].wFlags != XB_FLAGS_DO_NOT_SERIALIZE )
    {
        ChkDR( _DRM_XB_Serialize_Container( pcontextBuilder,
                                            pcontextBuilder->rgpObjectNodes[XB_OBJECT_GLOBAL_HEADER]->pvObject,
                                            pbBuffer,
                                           *f_pcbFormat,
                                           &iFormat ) );
    }

    if( pcontextBuilder->pformat->pHeaderDescription->pEntryDescription != NULL )
    {
        if( pcontextBuilder->pformat->pHeaderDescription->wOffsetOfSizeInHeaderStruct != DRM_XB_EXTENDED_HEADER_SIZE_OFFSET_ILWALID )
        {
            ChkDR( DRM_DWordAddSame( &ibExtHeader, pcontextBuilder->pformat->pHeaderDescription->wOffsetOfSizeInHeaderStruct ) );
            ChkDR( DRM_DWordSubSame( &ibExtHeader, sizeof(DRM_BOOL) ) ); /* Subtract fValid size since it is not serialized */
            AssertChkBOOL( ibExtHeader + sizeof( DRM_DWORD ) <= *f_pcbFormat );
            DWORD_TO_NETWORKBYTES( pbBuffer, ibExtHeader, iFormat );
        }
    }
    else
    {
        /* Update the total object size in the format header */
        ChkDR( DRM_DWordSub( XB_HEADER_LENGTH( pcontextBuilder->pformat->pHeaderDescription->eFormatIdLength ), sizeof( DRM_DWORD ), &dwResult ) );
        DWORD_TO_NETWORKBYTES( pbBuffer, dwResult, iFormat );
    }

    AssertChkBOOL( iFormat <= *f_pcbFormat );
    *f_pcbFormat = iFormat;

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
/******************************************************************************
**
** Function :   DRM_XB_SetExtendedHeader
**
** Synopsis :   Optional builder API used to set extended header data associated with
**              the XBinary format description. Not all formats require an extended
**              header.
**
** Arguments:   [f_pcontextBuilder] : The XBinary builder context.
**              [f_pbFormat       ] : The serialization buffer.  This parameter is optional.
**                                    If f_pbFormat is NULL, the serialization size will
**                                    be callwlated and returned via f_pcbFormat.
**              [f_pcbFormat      ] : On input, the size of the serialization buffer. On
**                                    output, the total size of the serialized data.
**
** Notes    :   The extended header information is stored in the last builder node which
**              was allocated in the DRM_XB_StartFormat.
**
******************************************************************************/
DRM_API DRM_RESULT DRM_CALL DRM_XB_SetExtendedHeader(
    __inout_ecount( 1 )    DRM_XB_BUILDER_CONTEXT    *f_pcontextBuilder,
    __in_ecount( 1 ) const DRM_VOID                  *f_pvObject )
{
    DRM_RESULT                       dr              = DRM_SUCCESS;
    DRM_XB_BUILDER_LISTNODE         *plistnode       = NULL;
    DRM_XB_BUILDER_CONTEXT_INTERNAL *pcontextBuilder =
                        DRM_REINTERPRET_CAST( DRM_XB_BUILDER_CONTEXT_INTERNAL, f_pcontextBuilder );

    ChkArg( f_pvObject != NULL );
    ChkArg( pcontextBuilder != NULL );
    ChkArg( pcontextBuilder->pformat->pHeaderDescription != NULL );
    ChkArg( pcontextBuilder->pformat->pHeaderDescription->pEntryDescription != NULL );
    ChkArg( pcontextBuilder->rgpObjectNodes != NULL );

    ChkDR( DRM_STK_Alloc( pcontextBuilder->pcontextStack,
                          sizeof( DRM_XB_BUILDER_LISTNODE ),
                          ( DRM_VOID ** )&plistnode ) );

    plistnode->Node.cbLength = pcontextBuilder->pformat->pHeaderDescription->pEntryDescription->dwStructureSize;
    plistnode->Node.pvObject = f_pvObject;
    plistnode->Node.wType    = XB_OBJECT_TYPE_EXTENDED_HEADER;
    plistnode->pNext         = NULL;
    pcontextBuilder->rgpObjectNodes[pcontextBuilder->pformat->cEntryDescriptions] = &plistnode->Node;

    /* Update overall XBinary size */
    ChkDR( DRM_DWordAddSame(
        &pcontextBuilder->rgpObjectNodes[XB_OBJECT_GLOBAL_HEADER]->cbLength,
         plistnode->Node.cbLength ) );

ErrorExit:
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;
