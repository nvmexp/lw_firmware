/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#pragma once
#include <lwtypes.h>
#include <libos.h>
#include "librbtree.h"
#include "memoryset.h"

typedef LwU8 PageStateRefenceCount;
#define PAGE_STATE_REFERENCE_COUNT_MAX 255

typedef struct {
    /**
     * Memory Pinning 
     *      referenceCount=1 is unpinned
     *      referenceCount>1 is pinned
     * 
     * @note: For 'MappableBuffers' the referenceCount is held
     *        by the MappableBuffer not the pagetables referencing it.
     *        For anonymous memory the PTE holds the reference count.
    */
    PageStateRefenceCount referenceCount;
    LwU8                  pageLog2;

    // Information area for use by allocator of page
    union
    {
        struct {
            LwU16 filledEntries;
        } pagetable;

        struct {
            // @todo: We could compress this to ~40 bits
            //        by leveraging the limited size of GPU physical addresses
            //        if we can assume this was mapped through the KernelPhysicalIdentityMap.
            struct MappableBuffer * mapping;
        } userMemory;
    };
} PageState;

LwU64       KernelMaxNaturalPageAtAddress(LwU64 address, LwU64 size);
PageState * KernelPageStateByAddress(LwU64 physicalAddress, struct MemoryPool ** outNode);
PageState * KernelPageStateContainingAddress(LwU64 address, struct MemoryPool * * outNode, LwU64 * physicalAddress);
PageState * KernelPageStateSplit(LwU64 physicalAddress, LwU64 size, struct MemoryPool ** outNode);

void        KernelPageStateMap(struct MemoryPool * node, LwU64 physicalAddress, LwU64 size, PageState state);
void        KernelPageStateMapNatural(struct MemoryPool * node, LwU64 physicalAddress, LwU64 size, PageState state);
