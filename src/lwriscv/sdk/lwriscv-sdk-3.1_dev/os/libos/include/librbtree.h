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

#endif // _BTREE_H_
