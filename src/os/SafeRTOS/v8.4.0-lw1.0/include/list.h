/*
    Copyright (C)2006 onward WITTENSTEIN aerospace & simulation limited. All rights reserved.

    This file is part of the SafeRTOS product, see projdefs.h for version number
    information.

    SafeRTOS is distributed exclusively by WITTENSTEIN high integrity systems,
    and is subject to the terms of the License granted to your organization,
    including its warranties and limitations on use and distribution. It cannot be
    copied or reproduced in any way except as permitted by the License.

    Licenses authorize use by processor, compiler, business unit, and product.

    WITTENSTEIN high integrity systems is a trading name of WITTENSTEIN
    aerospace & simulation ltd, Registered Office: Brown's Court, Long Ashton
    Business Park, Yanley Lane, Long Ashton, Bristol, BS41 9LB, UK.
    Tel: +44 (0) 1275 395 600, fax: +44 (0) 1275 393 630.
    E-mail: info@HighIntegritySystems.com

    www.HighIntegritySystems.com
*/

/*
    Functions for internal use only.  These functions are dolwmented within
    the software design description.  Do not refer to this file for
    documentation.
*/

#ifndef LIST_H
#define LIST_H

#ifdef __cplusplus
extern "C" {
#endif

struct xLIST_ITEM
{
    volatile portTickType xItemValue;
    struct xLIST_ITEM *volatile pxNext;
    struct xLIST_ITEM *volatile pxPrevious;
    void *pvOwner;
    void *volatile pvContainer;
};
typedef struct xLIST_ITEM xListItem;

struct xMINI_LIST_ITEM
{
    volatile portTickType xItemValue;
    struct xLIST_ITEM *volatile pxNext;
    struct xLIST_ITEM *volatile pxPrevious;
    void *pvOwner;
};
typedef struct xMINI_LIST_ITEM xMiniListItem;

typedef struct xLIST
{
    volatile portUnsignedBaseType uxNumberOfItems;
    xListItem *volatile pxIndex;
    xMiniListItem xListEnd;
} xList;


#define listSET_LIST_ITEM_OWNER( pxListItem, pxOwner )      ( pxListItem )->pvOwner = ( ( void * ) ( pxOwner ) )
#define listGET_LIST_ITEM_OWNER( pxListItem )               ( ( pxListItem )->pvOwner )
#define listSET_LIST_ITEM_VALUE( pxListItem, xValue )       ( pxListItem )->xItemValue = ( ( portTickType ) ( xValue ) )
#define listGET_LIST_ITEM_VALUE( pxListItem )               ( ( pxListItem )->xItemValue )
#define listGET_HEAD_ENTRY( pxList )                        ( ( ( pxList )->xListEnd ).pxNext )
#define listGET_NEXT( pxListItem )                          ( ( pxListItem )->pxNext )
#define listGET_END_MARKER( pxList )                        ( &( ( pxList )->xListEnd ) )
#define listLWRRENT_LIST_LENGTH( pxList )                   ( ( pxList )->uxNumberOfItems )
#define listGET_OWNER_OF_HEAD_ENTRY( pxList )               ( ( ( pxList )->uxNumberOfItems != ( portUnsignedBaseType ) 0 ) ? ( ( &( ( pxList )->xListEnd ) )->pxNext->pvOwner ) : ( NULL ) )
#define listGUARANTEED_GET_OWNER_OF_HEAD_ENTRY( pxList )    ( ( &( ( pxList )->xListEnd ) )->pxNext->pvOwner )
#define listGUARANTEED_GET_VALUE_OF_HEAD_ENTRY( pxList )    ( ( &( ( pxList )->xListEnd ) )->pxNext->xItemValue )

#define listGET_OWNER_OF_NEXT_ENTRY( pxTCB, pxList )                                \
    /* Increment the index to the next item and return the item, ensuring */        \
    /* we don't return the marker used at the end of the list.  */                  \
    {                                                                               \
        ( pxList )->pxIndex = ( pxList )->pxIndex->pxNext;                          \
        if( ( pxList )->pxIndex == ( xListItem * ) listGET_END_MARKER( pxList ) )   \
        {                                                                           \
            ( pxList )->pxIndex = ( pxList )->pxIndex->pxNext;                      \
            /* MCDC Test Point: STD_IF_IN_MACRO "listGET_OWNER_OF_NEXT_ENTRY" */    \
        }                                                                           \
        /* MCDC Test Point: ADD_ELSE_IN_MACRO "listGET_OWNER_OF_NEXT_ENTRY" */      \
        ( pxTCB ) = ( xTCB * ) ( pxList )->pxIndex->pvOwner;                        \
    }


KERNEL_CREATE_FUNCTION void vListInitialise( xList *pxList );
KERNEL_FUNCTION void vListInitialiseItem( xListItem *pxItem );
KERNEL_FUNCTION void vListInsertOrdered( xList *pxList, xListItem *pxNewListItem );
KERNEL_FUNCTION void vListInsertEnd( xList *pxList, xListItem *pxNewListItem );
KERNEL_FUNCTION void vListRemove( xListItem *pxItemToRemove );
KERNEL_FUNCTION void vListRelocateOrderedItem( xListItem *pxItemToRelocate, portTickType xNewListValue );
KERNEL_FUNCTION portBaseType xListIsListEmpty( const xList *pxList );
KERNEL_FUNCTION portBaseType xListIsContainedWithin( const xList *pxList, const xListItem *pxListItem );
KERNEL_FUNCTION portBaseType xListIsContainedWithinAList( const xListItem *pxListItem );

#ifdef __cplusplus
}
#endif

#endif /* LIST_H */
