/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "kernel.h"
#include <lwtypes.h>
#include "list.h"
#include "diagnostics.h"

void LIBOS_SECTION_IMEM_PINNED ListConstruct(ListHeader *id)
{
    id->next = id->prev = LIBOS_PTR32_NARROW(id);
}

LwBool LIBOS_SECTION_IMEM_PINNED ListEmpty(ListHeader * list)
{
    return list->next == LIBOS_PTR32_NARROW(list);
}

void LIBOS_SECTION_IMEM_PINNED ListLink(ListHeader *id)
{
    KernelAssert(LIBOS_PTR32_EXPAND(ListHeader, LIBOS_PTR32_EXPAND(ListHeader, id->prev)->next) != id && 
                 LIBOS_PTR32_EXPAND(ListHeader, LIBOS_PTR32_EXPAND(ListHeader, id->next)->prev) != id && "Already in list");

    ListHeader *next = LIBOS_PTR32_EXPAND(ListHeader, id->next);
    ListHeader *prev = LIBOS_PTR32_EXPAND(ListHeader, id->prev);
    next->prev          = LIBOS_PTR32_NARROW(id);
    prev->next          = LIBOS_PTR32_NARROW(id);
}

void LIBOS_SECTION_IMEM_PINNED ListUnlink(ListHeader *id)
{
    KernelAssert(LIBOS_PTR32_EXPAND(ListHeader, LIBOS_PTR32_EXPAND(ListHeader, id->prev)->next) == id && 
                 LIBOS_PTR32_EXPAND(ListHeader, LIBOS_PTR32_EXPAND(ListHeader, id->next)->prev) == id && "Not in list");

    ListHeader *next = LIBOS_PTR32_EXPAND(ListHeader, id->next);
    ListHeader *prev = LIBOS_PTR32_EXPAND(ListHeader, id->prev);
    prev->next          = LIBOS_PTR32_NARROW(next);
    next->prev          = LIBOS_PTR32_NARROW(prev);
}

LIBOS_SECTION_IMEM_PINNED void ListPushFront(ListHeader * list, ListHeader * element)
{
    element->prev = LIBOS_PTR32_NARROW(list);
    element->next = LIBOS_PTR32_NARROW(list->next);
    ListLink(element);
}

LIBOS_SECTION_IMEM_PINNED void ListPushBack(ListHeader * list, ListHeader * element)
{
    element->prev = LIBOS_PTR32_NARROW(list->prev);
    element->next = LIBOS_PTR32_NARROW(list);
    ListLink(element);
}


LIBOS_SECTION_IMEM_PINNED ListHeader * ListPopBack(ListHeader * list)
{
    ListHeader *tail = LIBOS_PTR32_EXPAND(ListHeader, list->prev);
    ListUnlink(tail);
    return tail;
}

LIBOS_SECTION_IMEM_PINNED ListHeader * ListPopFront(ListHeader * list)
{
    ListHeader *head = LIBOS_PTR32_EXPAND(ListHeader, list->next);
    ListUnlink(head);
    return head;
}

// @note Inserts at the end of all entries sharing the same value
LIBOS_SECTION_IMEM_PINNED void ListInsertSorted(ListHeader * header, ListHeader * target, LwS64 fieldFromHeader)
{
    LwU64 insertValue = *(LwU64 *)((LwU64)target + fieldFromHeader);
    
    // Start the insertion point at the end of the list
    ListHeader *insertBefore = header;

    // Find the insertion point 
    while (LIBOS_PTR32_EXPAND(ListHeader, insertBefore->prev) != header &&                                         
           insertValue < *(LwU64 *)((LwU64)insertBefore->prev + fieldFromHeader))      // <=?
    {
        insertBefore = LIBOS_PTR32_EXPAND(ListHeader, insertBefore->prev);
    }

    // Insert the element before insertBefore
    target->next = LIBOS_PTR32_NARROW(insertBefore);
    target->prev = LIBOS_PTR32_NARROW(insertBefore->prev);  
    ListLink(target);
}
