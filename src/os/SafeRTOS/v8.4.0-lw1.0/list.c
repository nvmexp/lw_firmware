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

/* Scheduler includes. */
#define KERNEL_SOURCE_FILE
#include "SafeRTOS.h"

/* MCDC Test Point: PROTOTYPE */

/*-----------------------------------------------------------------------------
 * PUBLIC LIST API dolwmented in list.h
 *---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION void vListInitialise( xList *pxList )
{
    /* MCDC Test Point: STD "vListInitialise" */

    /* The list structure contains a list item which is used to mark the
    end of the list.  To initialise the list the list end is inserted
    as the only list entry. */
    pxList->pxIndex = ( xListItem * ) listGET_END_MARKER( pxList );

    /* The list end value is the highest possible value in the list to
    ensure it remains at the end of the list. */
    pxList->xListEnd.xItemValue = portMAX_LIST_ITEM_VALUE;

    /* The list end next and previous pointers point to itself so we know
    when the list is empty. */
    pxList->xListEnd.pxNext = ( xListItem * ) listGET_END_MARKER( pxList );
    pxList->xListEnd.pxPrevious = ( xListItem * ) listGET_END_MARKER( pxList );

    /* The list head will never get used and has no owner. */
    pxList->xListEnd.pvOwner = NULL;

    pxList->uxNumberOfItems = ( portUnsignedBaseType ) 0;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vListInitialiseItem( xListItem *pxItem )
{
    /* MCDC Test Point: STD "vListInitialiseItem" */

    /* Make sure the list item is not recorded as being on a list. */
    pxItem->pvContainer = NULL;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vListInsertEnd( xList *pxList, xListItem *pxNewListItem )
{
    xListItem *pxIndex;

    /* MCDC Test Point: STD "vListInsertEnd" */

    /* Insert a new list item into pxList, but rather than sort the list,
    make the new list item the last item to be removed by a call to
    listGET_OWNER_OF_NEXT_ENTRY. This is achieved by adding the new item
    before the current item.*/

    pxIndex = pxList->pxIndex;

    pxNewListItem->pxNext = pxIndex;
    pxNewListItem->pxPrevious = pxIndex->pxPrevious;
    pxIndex->pxPrevious->pxNext = pxNewListItem;
    pxIndex->pxPrevious = pxNewListItem;

    /* Remember which list the item is in. */
    pxNewListItem->pvContainer = ( void * ) pxList;

    ( pxList->uxNumberOfItems )++;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vListInsertOrdered( xList *pxList, xListItem *pxNewListItem )
{
    xListItem *pxIterator;
    portTickType xValueOfInsertion;

    /* Insert the new list item into the list, sorted in xItemValue order. */
    xValueOfInsertion = pxNewListItem->xItemValue;

    /* If the list already contains a list item with the same item value then
    the new list item should be placed after it.  This ensures that TCB's which
    are stored in ready lists (all of which have the same xItemValue value)
    get an equal share of the CPU.  However, if the xItemValue is the same as
    the back marker the iteration loop below will not end.  This means we need
    to guard against this by checking the value first and simply making a direct
    assignment if appropriate. */
    if( portMAX_LIST_ITEM_VALUE == xValueOfInsertion )
    {
        /* MCDC Test Point: STD_IF "vListInsertOrdered" */

        pxIterator = pxList->xListEnd.pxPrevious;
    }
    else
    {
        /* MCDC Test Point: STD_ELSE "vListInsertOrdered" */

        /* Iterate to the wanted insertion position. */
        pxIterator = ( xListItem * ) listGET_END_MARKER( pxList );

        /* MCDC Test Point: WHILE_EXTERNAL "vListInsertOrdered" "( pxIterator->pxNext->xItemValue <= xValueOfInsertion )" */
        while( pxIterator->pxNext->xItemValue <= xValueOfInsertion )
        {
            pxIterator = pxIterator->pxNext;

            /* MCDC Test Point: WHILE_INTERNAL "vListInsertOrdered" "( pxIterator->pxNext->xItemValue <= xValueOfInsertion )" */
        }
    }

    pxNewListItem->pxNext = pxIterator->pxNext;
    pxNewListItem->pxNext->pxPrevious = pxNewListItem;
    pxNewListItem->pxPrevious = pxIterator;
    pxIterator->pxNext = pxNewListItem;

    /* Remember which list the item is in.  This allows fast removal of the
    item later. */
    pxNewListItem->pvContainer = ( void * ) pxList;

    ( pxList->uxNumberOfItems )++;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vListRemove( xListItem *pxItemToRemove )
{
    xList *pxList;
    /* Temporary pointer to prevent order-of-volatile-access warnings. */
    xListItem *pxTempItem;

    pxTempItem = pxItemToRemove->pxPrevious;
    pxItemToRemove->pxNext->pxPrevious = pxTempItem;
    pxTempItem = pxItemToRemove->pxNext;
    pxItemToRemove->pxPrevious->pxNext = pxTempItem;

    /* The list item knows which list it is in.
     * Obtain the list from the list item. */
    pxList = ( xList * ) pxItemToRemove->pvContainer;

    /* Make sure the index is left pointing to a valid item. */
    if( pxList->pxIndex == pxItemToRemove )
    {
        /* MCDC Test Point: STD_IF "vListRemove" */
        pxList->pxIndex = pxItemToRemove->pxPrevious;
    }
    /* MCDC Test Point: ADD_ELSE "vListRemove" */

    pxItemToRemove->pvContainer = NULL;
    ( pxList->uxNumberOfItems )--;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vListRelocateOrderedItem( xListItem *pxItemToRelocate, portTickType xNewListValue )
{
    xList *pxList;

    /* Update the value of the list item. */
    pxItemToRelocate->xItemValue = xNewListValue;

    /* The list item knows which list it is in.  Obtain the list from the list
    item. */
    pxList = ( xList * ) pxItemToRelocate->pvContainer;

    /* If the item is in a list, remove and re-add to ensure that it goes into
     * the correct location within the ordered list. */
    if( NULL != pxList )
    {
        /* Remove from list. */
        vListRemove( pxItemToRelocate );

        /* Add back in to the list. */
        vListInsertOrdered( pxList, pxItemToRelocate );

        /* MCDC Test Point: STD_IF "vListRelocateOrderedItem" */
    }
    /* MCDC Test Point: ADD_ELSE "vListRelocateOrderedItem" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xListIsListEmpty( const xList *pxList )
{
    portBaseType xReturn;

    if( ( portUnsignedBaseType ) 0U == pxList->uxNumberOfItems )
    {
        xReturn = pdTRUE;

        /* MCDC Test Point: STD_IF "xListIsListEmpty" */
    }
    else
    {
        xReturn = pdFALSE;

        /* MCDC Test Point: STD_ELSE "xListIsListEmpty" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xListIsContainedWithin( const xList *pxList, const xListItem *pxListItem )
{
    portBaseType xReturn;

    if( pxListItem->pvContainer == ( void * ) pxList )
    {
        xReturn = pdTRUE;

        /* MCDC Test Point: STD_IF "xListIsContainedWithin" */
    }
    else
    {
        xReturn = pdFALSE;

        /* MCDC Test Point: STD_ELSE "xListIsContainedWithin" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xListIsContainedWithinAList( const xListItem *pxListItem )
{
    portBaseType xReturn;

    if( pxListItem->pvContainer != NULL )
    {
        xReturn = pdTRUE;

        /* MCDC Test Point: STD_IF "xListIsContainedWithinAList" */
    }
    else
    {
        xReturn = pdFALSE;

        /* MCDC Test Point: STD_ELSE "xListIsContainedWithinAList" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/


#ifdef SAFERTOS_TEST
    #include "SectionLocationCheckList_listc.h"
#endif

#ifdef SAFERTOS_MODULE_TEST
    #include "ListCTestHeaders.h"
    #include "ListCTest.h"
#endif

