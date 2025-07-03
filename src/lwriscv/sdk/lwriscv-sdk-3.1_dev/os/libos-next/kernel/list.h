#pragma once
#include "compressed-pointer.h"

typedef struct ListHeader
{
    LIBOS_PTR32(struct ListHeader) next;
    LIBOS_PTR32(struct ListHeader) prev;
} ListHeader;

LIBOS_SECTION_IMEM_PINNED void         ListConstruct(ListHeader *id);
LIBOS_SECTION_IMEM_PINNED void         ListLink(ListHeader *id);
LIBOS_SECTION_IMEM_PINNED void         ListUnlink(ListHeader *id);
LIBOS_SECTION_IMEM_PINNED void         ListPushFront(ListHeader * list, ListHeader * element);
LIBOS_SECTION_IMEM_PINNED void         ListPushBack(ListHeader * list, ListHeader * element);
LIBOS_SECTION_IMEM_PINNED LwBool       ListEmpty(ListHeader * list);

#define LIST_INSERT_SORTED(list, element, type, headerName, valueName) \
    ListInsertSorted(list, element, (int)offsetof(type, valueName)-(int)offsetof(type,headerName))

#define LIST_POP_BACK(list, type, headerName) \
    CONTAINER_OF(ListPopBack(&list), type, headerName);

#define LIST_POP_FRONT(list, type, headerName) \
    CONTAINER_OF(ListPopFront(&list), type, headerName);

// Internal API
LIBOS_SECTION_IMEM_PINNED void ListInsertSorted(ListHeader * header, ListHeader * element, LwS64 fieldFromHeader);
LIBOS_SECTION_IMEM_PINNED ListHeader * ListPopBack(ListHeader * list);
LIBOS_SECTION_IMEM_PINNED ListHeader * ListPopFront(ListHeader * list);

