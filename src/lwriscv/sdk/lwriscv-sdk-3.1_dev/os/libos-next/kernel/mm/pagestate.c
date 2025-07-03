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
#include "libos.h"
#include <lwtypes.h>
#include "buddy.h"
#include "pagetable.h"
#include "diagnostics.h"
#include "mm/memorypool.h"
#include "libbit.h"
#include "mm/objectpool.h"
#include "task.h"
#include "scheduler.h"
#include "libriscv.h"


/**
 * @brief Finds the largest power of two size page at address.
 * 
 * Algorithm:
 *    AddressSet = Page sizes that divide the address
 *    SizeSet    = Page sizes less than or equal to size 
 *    BlockSize = intersect(AddressSet, SizeSet)
 * 
 *    We can pick the largest page in BlockSet.
 * 
 * Address Set:
 *    LibosBitLowest(address) returns mask of the lowest bit set
 *                            this is the largest power of 2 page
 *                            of which address is naturally aligned to. 
 * 
 * Size Set:
 *     LibosBitHighest(size)  returns the largest power of 2 page size
 *                            less than or equal to the page size.
 * 
 * @param address 
 * @param size 
 * @return LwU64 
 */
LwU64 KernelMaxNaturalPageAtAddress(LwU64 address, LwU64 size)
{
    LwU64 sizeSet = LibosBitAndBitsBelow(LibosBitHighest(size));
    LwU64 addressSet = LibosBitAndBitsBelow(LibosBitLowest(address));
    return LibosBitHighest(sizeSet & addressSet);
}

/*
    Single NUMA node support:   
        - Memory may not be registered later
        - Holes are allowed
        - Page state directory is stored discontiguously

    Ilwariants:
        1. pageState(pageBaseAddress) contains the description of a given page
        2. pageState(pageBaseAddress+k) where k is inside the page contains
            buddyFree == LW_TRUE.
        3. Buddy tree allocations are always power of 2 pages and correspond
           one to one with a pagestate entry.
    
    Implications:
        1. We can find the address containing a page by rounding the address
           down to progressively larger powers of 2 until we find an allocation
*/

/**
 * @brief Locates the page state structure for a page within a MemoryPool.
 * 
 * PageState provides context dependent storage for data that doesn't readily
 * fit within a fix sized allocation.
 * 
 * For instance, page state is used for PDE and PTE pages to track the number of filled entries
 * for resource reclaimation.  
 * 
 * @param physicalAddress 
 * @param outNode 
 * @return PageState* 
 */
PageState * KernelPageStateByAddress(LwU64 physicalAddress, MemoryPool ** outNode)
{
    MemoryPool * node = KernelMemoryPoolFind(physicalAddress);

    if (outNode)
        *outNode = node;

    if (!node)
        return 0;

    return &node->pageState[(physicalAddress - node->physicalBase) >> 12];
}

/**
 * @brief Finds the page containing this address and returns the state
 * 
 * @note KernelPageStateByAddress requires an exact address, 
 *       this only requires an address within the page of unknown size.
 * 
 * @param address 
 * @param outNode 
 * @param physicalAddress 
 * @return PageState* 
 */
PageState * KernelPageStateContainingAddress(LwU64 address, MemoryPool * * outNode, LwU64 * physicalAddress)
{
    LwU64 mask = ((1ULL << 12)-1);
    do
    {
        PageState * state = KernelPageStateByAddress(address &~ mask, outNode);

        if (!state || state->referenceCount==0) 
        {
            // If we already tried the lowest address, stop the search
            if ((address & mask) == 0)
                return 0;

            // Mask off an additional bit at the top
            if (mask == -1ULL)
                return 0;
            mask = 2 * mask + 1;
            continue;
        }

        // Found it!
        *physicalAddress = address &~ mask;

        return state;
    } while(LW_FALSE);

    return 0;
}

/**
 * @brief Creates a page-state mapping for the address range
 * 
 * Decomposes the memory region into naturally aligned power of two addresses.
 * 
 * @param node 
 * @param physicalAddress 
 * @param size 
 * @param template 
 */
void KernelPageStateMap(MemoryPool * node, LwU64 physicalAddress, LwU64 size, PageState template)
{
    size = LibosRoundUp(size, LIBOS_CONFIG_PAGESIZE);

    // Perform the allocation on all power of two sections
    while (size)
    {
        // Find the largest naturally aligned power of two page
        LwU64 chunkSize = KernelMaxNaturalPageAtAddress(physicalAddress, size);
        LwU64 chunkLog2 = LibosLog2GivenPow2(chunkSize);

        // Look up the page state structure at the start of that page
        PageState * pageState = KernelPageStateByAddress(physicalAddress, 0);   

        // Mark it as allocated
        KernelPanilwnless(pageState && pageState->referenceCount == 0);
        *pageState = template;
        pageState->pageLog2 = chunkLog2;

        physicalAddress += chunkSize;
        size            -= chunkSize;
    }
}

void KernelPageStateMapNatural(MemoryPool * node, LwU64 physicalAddress, LwU64 size, PageState state)
{
    // Must be a naturally aligned page
    KernelAssert(KernelMaxNaturalPageAtAddress(physicalAddress, size) == size);
    PageState * pageState = KernelPageStateByAddress(physicalAddress, 0);
    KernelPanilwnless(pageState && pageState->referenceCount == 0);
    *pageState = state;
    pageState->pageLog2 = LibosLog2GivenPow2(size);
}


/**
 * @brief Looks up the page state for an allocation covering 
 *        the physical address range.  Splits any pages
 *        that aren't wholly inside or outside of the range.
 * 
 *        Returns the pagestate for the first naturally aligned 
 *        block in the region.
 *  
 * @param physicalAddress 
 * @param size 
 * @param delta 
 */
PageState * KernelPageStateSplit(LwU64 physicalAddress, LwU64 size, struct MemoryPool ** outNode)
{
    // Find the naturally aligned page containing the physicalAddress
    LwU64 pageAddress;
    MemoryPool * node = 0;
    PageState * state = KernelPageStateContainingAddress(physicalAddress, &node, &pageAddress);
    *outNode = node;

    // This isn't a valid page
    if (!state) 
        return 0;

    PageState template = *state;

    LwU64 pageSize = 1ULL << state->pageLog2;

    // Is the current page entirely within the search range?
    // If so, we don't need to split
    if (physicalAddress == pageAddress && pageSize <= size)
        return state;

    // Clear out the superpage in page-state
    state->referenceCount = 0;

    // Break into smaller pages in the page-state
    KernelPageStateMap(node, pageAddress, physicalAddress - pageAddress, template);
    KernelPageStateMap(node, physicalAddress, size, template);
    KernelPageStateMap(node, physicalAddress+size, pageAddress + pageSize - physicalAddress - size, template);

    // There state object has moved.
    return KernelPageStateByAddress(physicalAddress, &node);
}
