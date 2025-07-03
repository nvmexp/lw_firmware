/*
 * OpenRTOS - Copyright (C) Wittenstein High Integrity Systems.
 *
 * OpenRTOS is distributed exclusively by Wittenstein High Integrity Systems,
 * and is subject to the terms of the License granted to your organization,
 * including its warranties and limitations on distribution.  It cannot be
 * copied or reproduced in any way except as permitted by the License.
 *
 * Licenses are issued for each conlwrrent user working on a specified product
 * line.
 *
 * WITTENSTEIN high integrity systems is a trading name of WITTENSTEIN
 * aerospace & simulation ltd, Registered Office: Brown's Court, Long Ashton
 * Business Park, Yanley Lane, Long Ashton, Bristol, BS41 9LB, UK.
 * Tel: +44 (0) 1275 395 600, fax: +44 (0) 1275 393 630.
 * E-mail: info@HighIntegritySystems.com
 * Registered in England No. 3711047; VAT No. GB 729 1583 15
 *
 * http://www.HighIntegritySystems.com
 */

#include <stdlib.h>
#include "OpenRTOS.h"
#include "list.h"

/*-----------------------------------------------------------
 * PUBLIC LIST API dolwmented in list.h
 *----------------------------------------------------------*/

void vListInitialise( xList *pxList )
{
    /* The list structure contains a list item which is used to mark the
    end of the list.  To initialise the list the list end is inserted
    as the only list entry. */
    pxList->pxIndex = ( xListItem * )(void *)&( pxList->xListEnd );

    /* The list end value is the highest possible value in the list to
    ensure it remains at the end of the list. */
    pxList->xListEnd.xItemValue = portMAX_DELAY;

    /* The list end next and previous pointers point to itself so we know
    when the list is empty. */
    pxList->xListEnd.pxNext     = ( xListItem * )(void *)&( pxList->xListEnd );
    pxList->xListEnd.pxPrevious = ( xListItem * )(void *)&( pxList->xListEnd );

    pxList->uxNumberOfItems = 0;
}
/*-----------------------------------------------------------*/

void vListInitialiseItem( xListItem *pxItem )
{
    /* Make sure the list item is not recorded as being on a list. */
    pxItem->pvContainer = NULL;
}
/*-----------------------------------------------------------*/

void vListInsertEnd( xList *pxList, xListItem *pxNewListItem )
{
volatile xListItem * pxIndex;

    /* Insert a new list item into pxList, but rather than sort the list,
    makes the new list item the last item to be removed by a call to
    pvListGetOwnerOfNextEntry.  This means it has to be the item pointed to by
    the pxIndex member. */
    pxIndex = pxList->pxIndex;

    pxNewListItem->pxNext = pxIndex->pxNext;
    pxNewListItem->pxPrevious = pxList->pxIndex;
    pxIndex->pxNext->pxPrevious = ( volatile xListItem * ) pxNewListItem;
    pxIndex->pxNext = ( volatile xListItem * ) pxNewListItem;
    pxList->pxIndex = ( volatile xListItem * ) pxNewListItem;

    /* Remember which list the item is in. */
    pxNewListItem->pvContainer = ( void * ) pxList;

    ( pxList->uxNumberOfItems )++;
}
/*-----------------------------------------------------------*/

void vListInsert( xList *pxList, xListItem *pxNewListItem )
{
volatile xListItem *pxIterator;
portTickType xValueOfInsertion;

    /* Insert the new list item into the list, sorted in ulListItem order. */
    xValueOfInsertion = pxNewListItem->xItemValue;

    /* If the list already contains a list item with the same item value then
    the new list item should be placed after it.  This ensures that TCB's which
    are stored in ready lists (all of which have the same ulListItem value)
    get an equal share of the CPU.  However, if the xItemValue is the same as
    the back marker the iteration loop below will not end.  This means we need
    to guard against this by checking the value first and modifying the
    algorithm slightly if necessary. */
    if( xValueOfInsertion == portMAX_DELAY )
    {
        pxIterator = pxList->xListEnd.pxPrevious;
    }
    else
    {
        for( pxIterator = ( xListItem * )(void *)&( pxList->xListEnd ); pxIterator->pxNext->xItemValue <= xValueOfInsertion; pxIterator = pxIterator->pxNext )
        {
            /* There is nothing to do here, we are just iterating to the
            wanted insertion position. */
        }
    }

    pxNewListItem->pxNext = pxIterator->pxNext;
    pxNewListItem->pxNext->pxPrevious = ( volatile xListItem * ) pxNewListItem;
    pxNewListItem->pxPrevious = pxIterator;
    pxIterator->pxNext = ( volatile xListItem * ) pxNewListItem;

    /* Remember which list the item is in.  This allows fast removal of the
    item later. */
    pxNewListItem->pvContainer = ( void * ) pxList;

    ( pxList->uxNumberOfItems )++;
}
/*-----------------------------------------------------------*/

void vListRemove( xListItem *pxItemToRemove )
{
xList * pxList;

    pxItemToRemove->pxNext->pxPrevious = pxItemToRemove->pxPrevious;
    pxItemToRemove->pxPrevious->pxNext = pxItemToRemove->pxNext;

    /* The list item knows which list it is in.  Obtain the list from the list
    item. */
    pxList = ( xList * ) pxItemToRemove->pvContainer;

    /* Make sure the index is left pointing to a valid item. */
    if( pxList->pxIndex == pxItemToRemove )
    {
        pxList->pxIndex = pxItemToRemove->pxPrevious;
    }

    pxItemToRemove->pvContainer = NULL;
    ( pxList->uxNumberOfItems )--;
}
/*-----------------------------------------------------------*/

