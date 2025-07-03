/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 1993-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited. 
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBRBTREE_H_
#define LIBRBTREE_H_
#include <lwtypes.h>

typedef struct LibosTreeHeader
{
    struct LibosTreeHeader      *parent;
    struct LibosTreeHeader      *left;
    struct LibosTreeHeader      *right;

    LwBool                       isRed;   
    LwBool                       isNil;         
} LibosTreeHeader;

void LibosTreeConstruct(LibosTreeHeader *header);
void LibosTreeInsertFixup(LibosTreeHeader *x, void (*mutator)(LibosTreeHeader * node));
void LibosTreeUnlink(LibosTreeHeader *, void (*mutator)(LibosTreeHeader * node));
LibosTreeHeader * LibosTreeRoot(LibosTreeHeader * i);
LibosTreeHeader * LibosTreeLeftmost(LibosTreeHeader * i);
LibosTreeHeader * LibosTreeSuccessor(LibosTreeHeader * i);

#define LIBOS_TREE_SIMPLE(ContainerType, containerField, NodeType, keyField, KeyType)               \
    static NodeType * ContainerType##Find##NodeType(ContainerType * container, KeyType keyValue)    \
    {                                                                                               \
        LibosTreeHeader * slot = container->containerField.parent;                                  \
        while (!slot->isNil) {                                                                      \
            NodeType * node = CONTAINER_OF(slot, NodeType, header);                                 \
            if (keyValue < node->keyField)                                                          \
                slot = slot->left;                                                                  \
            else if (keyValue > node->keyField)                                                     \
                slot = slot->right;                                                                 \
            else                                                                                    \
                return node;                                                                        \
        }                                                                                           \
        return 0;                                                                                   \
    }                                                                                               \
                                                                                                    \
    static void ContainerType##Insert##NodeType(ContainerType * container, NodeType * node)         \
    {                                                                                               \
        LibosTreeHeader * * slot = &container->containerField.parent;                               \
        node->header.parent = node->header.left = node->header.right = &container->containerField;  \
                                                                                                    \
        while (!(*slot)->isNil)                                                                     \
        {                                                                                           \
            node->header.parent = *slot;                                                            \
            if (CONTAINER_OF(node, NodeType, header)->mapping < CONTAINER_OF(*slot, NodeType, header)->mapping) \
                slot = &(*slot)->left;                                                              \
            else                                                                                    \
                slot = &(*slot)->right;                                                             \
        }                                                                                           \
        *slot = &node->header;                                                                      \
                                                                                                    \
        LibosTreeInsertFixup(&node->header, 0);                                                     \
    }

#endif // _BTREE_H_
