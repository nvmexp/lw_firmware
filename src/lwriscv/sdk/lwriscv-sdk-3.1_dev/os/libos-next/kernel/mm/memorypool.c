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
 * @brief The default memory pool for use by the kernel.
 * 
 * @see init.c 
 * 
 */
OWNED_PTR MemorySet * KernelPoolMemorySet;


/**
 * @brief All memory pools are registered with this tree 
 * 
 * This enables locating a MemoryPool from a physical address,
 * primarily used for locating metadata such as the page state array.
 * 
 * @note This tree holds a reference count on the memory
 *       node to prevent reclamation. 
 * 
 */
static LibosTreeHeader memoryPoolsByAddress;

/**
 * @brief Registers a memory pool into the internal address tracking tree.
 * 
 * @param node 
 */
static void KernelMemoryPoolRegister(OWNED_PTR MemoryPool * node)
{
    LibosTreeHeader * * slot = &memoryPoolsByAddress.parent;
    node->header.parent = node->header.left = node->header.right = &memoryPoolsByAddress;
    
    while (!(*slot)->isNil)
    {
        node->header.parent = *slot;
        if (node->physicalBase < CONTAINER_OF(*slot, MemoryPool, header)->physicalBase)
            slot = &(*slot)->left;
        else
            slot = &(*slot)->right;
    }
    *slot = &node->header;

    LibosTreeInsertFixup(&node->header, 0);
}

/**
 * @brief Locates the memory pool containing the physical address.
 * 
 * @param address - Physical address with the peregrine RISC-V aperture included
 * @return MemoryPool* 
 */
MemoryPool * KernelMemoryPoolFind(LwU64 address)
{
    LibosTreeHeader * i = memoryPoolsByAddress.parent;

    while (!i->isNil)
        // The address is before the start of this region
        if (address < CONTAINER_OF(i, MemoryPool, header)->physicalBase)
            i = i->left;
        // The address is after the end of the region
        else if (address > CONTAINER_OF(i, MemoryPool, header)->physicalLast)
            i = i->right;
        else 
            return CONTAINER_OF(i, MemoryPool, header);

    return 0;
}



typedef enum {
    ContiguousAllocateExact,                        // Allocate the exact address and create PageState
    ContiguousAllocateAllocateAfter,                 // Allocate nearest and create page state
    ContiguousAllocateExactWithoutPageState         // Perform buddy allocation without creating page state
} AllocationMode;


/**
 * @brief Performs a contiguous memory allocation on the MemoryPool
 *  
 *  Handles decomposing the allocation into the minimum number of naturally aligned
 *  power of 2 pages that cover the space.
 * 
 *  Allocates the pages from the buddy allocator and creates the appropriate 
 *  pagestate to track these pages.
 * 
 *  
 * 
 * @param node 
 * @param probingMode 
 * @param searchAddress 
 * @param size 
 * @param physicalAddress 
 * @param referenceCount 
 * @return LibosStatus 
 */
static LibosStatus KernelMemoryPoolAllocateContiguousInternal(MemoryPool * node, AllocationMode probingMode, LwU64 searchAddress, LwU64 size, LwU64 * physicalAddress, LwU64 referenceCount)
{
    if (!size)
        return LibosOk;

    Buddy * buddy = node->buddyTree;
    BuddyIterator superblock;

    LwU64 candidateAddress = searchAddress;
    // Probe to find the largest power of two page in the allocation
    do
    {
        LwU64 probeAddress = candidateAddress;
        LwU64 remainingSize = size;

        // Are size bytes free at the candidate address?
        do
        {
            LwU64 chunkSize = KernelMaxNaturalPageAtAddress(probeAddress, remainingSize);

            LwU64 matchAddress;
            if (!KernelBuddyFindFreeByAddress(buddy, probeAddress, LibosBitLog2Floor(chunkSize), &superblock, &matchAddress) || 
                matchAddress != probeAddress)
                goto advanceSearch;

            if (__builtin_add_overflow(probeAddress, chunkSize, &probeAddress))
                return LibosErrorOutOfMemory;

            // Proof cannot underflow
            //     KernelMaxNaturalPageAtAddress(... , remainingSize) <= remainingSize 
            //     thus chunkSize <= remainingSize
            remainingSize -= chunkSize;
        } while(remainingSize);

        // Yes! Allocate size bytes at the candidateAddress
        LwU64 commitAddress = candidateAddress;
        remainingSize = size;
        while (remainingSize)
        {
            LwU64 chunkSize = KernelMaxNaturalPageAtAddress(commitAddress, remainingSize);

            // Mark the region as allocated in the buddy
            LwU64 free;
            KernelPanilwnless(KernelBuddyAllocate(buddy, commitAddress, &free, LibosBitLog2Floor(chunkSize)) && free==commitAddress);

            // Update the page state tree
            if (probingMode != ContiguousAllocateExactWithoutPageState)
            {
                PageState state;
                state.referenceCount = referenceCount;
                KernelPageStateMapNatural(node, commitAddress, chunkSize, state);
            }

            // Proof this can't overflow: We've already looped over the candidateAddress through
            // the full size in the probing step.  This loop is identical.
            commitAddress += chunkSize;
            remainingSize -= chunkSize;
        }

        if (physicalAddress)
            *physicalAddress = candidateAddress;
        
        return LibosOk;

    advanceSearch:;
        if (probingMode != ContiguousAllocateAllocateAfter)
            return LibosErrorOutOfMemory;

        // Heuristic: Start the search at the next free page.
        // @todo: We could be smarter here for larger allocations
        //        A 2mb contiguous allocation must contain at least one aligned 1mb page.
        //        based on this we could scan for free pages of half the size 
        //        and walk backwards until we find the first allocated page.
        LwU64 matchAddress;
        if (!KernelBuddyFindFreeByAddress(buddy, candidateAddress, buddy->minimumPageLog2, &superblock, &matchAddress) || 
            matchAddress < candidateAddress)
            return LibosErrorOutOfMemory;
        candidateAddress = matchAddress;
        
    } while (LW_TRUE);

    return LibosErrorOutOfMemory;
}

/**
 * @brief Creates a new memory pool at the physical address range.
 * 
 * A memory pool is a source of pages for all allocations in both kernel
 * and user-space.  
 * 
 * Memory pools are composed of two subsystems
 * 
 * Buddy allocator - An optimized O(lg N) flat tree for quickly locating
 *                   and allocating naturally sized and aligned pages
 * 
 * PageState array - This data structure allows finding the metadata
 *                   for a page given just its address [and not size].
 * 
 * @param reqPhysicalBase 
 * @param reqPhysicalSize 
 * @param physicalMemoryNode 
 * @return LibosStatus 
 */
LibosStatus KernelAllocateMemoryPool(LwU64 reqPhysicalBase, LwU64 reqPhysicalSize, MemoryPool * * physicalMemoryNode)
{
    MemoryPool * node = KernelPoolAllocate(LIBOS_POOL_MEMORY_POOL);

    /*
        The buddy allocator sees a padded version of the node that is 
        a naturally aligned power of 2 sized block.

        The buddy allocator tracks all memory within this "super-block".

        We place the Buddy structure and tracking array at the beginning
        of of the actual memory region.

        We mark the 'Pre' and 'Post' padding regions that were added
        to make the region naturally aligned power of 2 as allocated
        in the buddy tree.


        ---------------------- <-  Power of 2 aligned
        |   Pre Padding      |
        |____________________| <-- reqPhysicalAddress
        |   Buddy Struct     |
        |--------------------|
        |   Buddy Tree Array |
        |                    |
        |--------------------|
        |                    |
        |                    |
        |   Free Memory      |
        |                    |
        |                    |
        |--------------------| <-- reqPhysicalAddress + reqPhysicalSize
        |    Post Padding    |
        ---------------------- <- Power of 2 aligned
    */

    // Compute the physical address of the last byte being onlined
    LwU64 reqPhysicalLast;
    if (!reqPhysicalSize || __builtin_add_overflow(reqPhysicalBase, reqPhysicalSize - 1, &reqPhysicalLast))
        return LibosErrorArgument;

    /**
     * @brief Find a naturally aligned power of two page
     *        containing the onlined address.  This is required
     *        since our buddy allocator only supports natural nodes.
     */

    // Round the requested size up to the next power of 2
    if (reqPhysicalSize > (1ULL << 63))
        return LibosErrorArgument;      // LibosBitRoundUpPower2 might overflow
    LwU64 buddyPhysicalSize = LibosBitRoundUpPower2(reqPhysicalSize);

    // Align the start address down to be naturally aligned
    LwU64 buddyPhysicalBase = LibosRoundDown(reqPhysicalBase, buddyPhysicalSize);

    // Find the last byte in the prescribe buddy node
    LwU64 buddyPhysicalLast;
    if (__builtin_add_overflow(buddyPhysicalBase, buddyPhysicalSize, &buddyPhysicalLast))
        return LibosErrorArgument;

    // Since we rounded down we subtracted at most physicalSize-1
    // we may need to increase the size by physicalSize to ensure
    // the end is still within the allocation.
    // 
    // At most one iteration is required.
    if (buddyPhysicalLast < reqPhysicalLast) {
        buddyPhysicalSize <<= 1;
        buddyPhysicalBase = LibosRoundDown(reqPhysicalBase, buddyPhysicalSize);
        if (__builtin_add_overflow(buddyPhysicalBase, buddyPhysicalSize, &buddyPhysicalLast))
            return LibosErrorArgument;
    }

    // Ensure containment
    KernelPanilwnless(reqPhysicalBase >= buddyPhysicalBase &&
                      reqPhysicalLast <= buddyPhysicalLast);

    // Fill out the MemoryPool
    node->physicalBase = reqPhysicalBase;
    node->physicalLast = reqPhysicalLast;

    // Compute the buddy tree location
    // Bind the remainder of the memory to the buddy allocator
    LwU64 treeSize = KernelBuddyTreeSize(buddyPhysicalBase, buddyPhysicalSize);
    
    // Compute memory requirements for page state
    LwU64 pageStateElements = LibosRoundUp(reqPhysicalSize, LIBOS_CONFIG_PAGESIZE) >> LIBOS_CONFIG_PAGESIZE_LOG2; 
    LwU64 pageStateSize;
    if (__builtin_mul_overflow(pageStateElements, sizeof(PageState), &pageStateSize))
        return LibosErrorArgument;

    // Ensure the buddy tree fits within the requested block
    LwU64 totalSize = sizeof(Buddy);
    if (__builtin_add_overflow(totalSize, treeSize, &totalSize))
        return LibosErrorArgument;
    if (__builtin_add_overflow(totalSize, pageStateSize, &totalSize))
        return LibosErrorArgument;

    // Does this leave any free space in the MemoryPool
    if (totalSize >= reqPhysicalSize)
        return LibosErrorOutOfMemory;

    // The buddy structure is at the front of the NUMA node
    node->buddyTree = (Buddy *)KernelPhysicalIdentityMap(reqPhysicalBase);

    // The buddy tree array is next
    // @proof Can't overflow since we're addressing a valid address within
    //        our allocation.
    LwU64 * buddyArray = (LwU64 *)(node->buddyTree + 1); 

    PageState * pageState = (PageState *)(buddyArray + treeSize/sizeof(LwU64));
    node->pageState = pageState;

    KernelPrintf("Libos: Construct NUMA page state tables.\n");

    // Create the page tracking structures
    for (LwU64 i = 0; i != (reqPhysicalSize >> LIBOS_CONFIG_PAGESIZE_LOG2); i++)
        node->pageState[i].referenceCount = 0;
    
    // Construct the Buddy tree 
    KernelBuddyConstruct(node->buddyTree, buddyArray, buddyPhysicalBase, buddyPhysicalSize, LIBOS_CONFIG_PAGESIZE_LOG2);

    // Trim any space at the start
    LwU64 paddingAtStart = LibosRoundUp(reqPhysicalBase + totalSize, LIBOS_CONFIG_PAGESIZE) - buddyPhysicalBase;
    KernelPanilwnless(LibosOk == KernelMemoryPoolAllocateContiguousInternal(node, ContiguousAllocateExactWithoutPageState, buddyPhysicalBase, paddingAtStart, 0, 0));

    // Trim any space at the end
    LwU64 paddingAtEnd = buddyPhysicalBase + buddyPhysicalSize - LibosRoundDown(reqPhysicalBase+reqPhysicalSize, LIBOS_CONFIG_PAGESIZE);
    KernelPanilwnless(LibosOk == KernelMemoryPoolAllocateContiguousInternal(node, ContiguousAllocateExactWithoutPageState, reqPhysicalBase + reqPhysicalSize, paddingAtEnd, 0, 0));

    KernelPrintf("Libos: NUMA memory node registered %llx-%llx\n", reqPhysicalBase, reqPhysicalBase + reqPhysicalSize);  

    // Register the memory node (this global data structure holds on to this physical memory node forever)
    // Offlining would theoretically handle the removal
    KernelPoolAddref(node);
    KernelMemoryPoolRegister(node);
    *physicalMemoryNode = node;

    return LibosOk;
}


/**
 * @brief User-space syscall wrapper for KernelAllocateMemoryPool
 * 
 * @param physicalBase 
 * @param physicalSize 
 * @return LibosStatus 
 */
LibosStatus KernelSyscallMemoryPoolAllocate(LwU64 physicalBase, LwU64 physicalSize) 
{
    Task * self = KernelSchedulerGetTask();
    MemoryPool * node;

    // Security: Only the init task is allowed to online new MemoryPool's
    if (KernelSchedulerGetTask() != TaskInit)
        return LibosErrorFailed;

    // Allocate the kernel object
    LibosStatus status = KernelAllocateMemoryPool(physicalBase, physicalSize, &node);
    if (status != LibosOk)
        return status;

    // Map it to user-space
    LibosMemoryPoolHandle handle;
    status = KernelTaskRegisterObject(self, &handle, node, LibosGrantMemoryPoolAll, 0);
    KernelPoolRelease(node);

    // Return the value
    self->state.registers[RISCV_T0] = handle;

    return status;
}


/**
 * @brief Allocates a naturally aligned power of 2 sized page from the pool
 * 
 * This operation is guaranteed succeed in O(m lg n) time where m is in the 
 * number of MemoryPools in the memorySet.
 * 
 * @param memorySet 
 * @param pageLog2 
 * @param physicalAddress 
 * @return LibosStatus 
 */
LibosStatus KernelMemoryPoolAllocatePage(MemorySet * memorySet, LwU64 pageLog2, LwU64 * physicalAddress)
{
    LibosTreeHeader * i = LibosTreeLeftmost(LibosTreeRoot(memorySet));
    while (!i->isNil) 
    {
        MemoryPoolSetEntry * entry = CONTAINER_OF(i, MemoryPoolSetEntry, header);
        i = LibosTreeSuccessor(i);


        if (!KernelBuddyAllocate(entry->node->buddyTree, 0, physicalAddress, pageLog2))
            continue;   
        //KernelPrintf("Buddy: Allocating page %llx\n", *physicalAddress);

        // Create the PageState for this allocation
        PageState template = {0};
        template.referenceCount = 1;
        template.pageLog2 = pageLog2;
        KernelPageStateMapNatural(entry->node, *physicalAddress, 1ULL << pageLog2, template);
        
        //KernelPrintf("Pool: Map %llx/%llx\n", *physicalAddress, 1ULL << pageLog2);
        return LibosOk;
    }

    return LibosErrorOutOfMemory;
}

/**
 * @brief Allocates a region of contiguous memory from the pool
 * 
 * @param memorySet 
 * @param size 
 * @param physicalAddress 
 * @return LibosStatus 
 */
LibosStatus KernelNumaAllocateContiguous(MemorySet * memorySet, LwU64 size, LwU64 * physicalAddress)
{
    size = LibosRoundUp(size, LIBOS_CONFIG_PAGESIZE);
    LibosTreeHeader * i = LibosTreeLeftmost(LibosTreeRoot(memorySet));
    while (!i->isNil) 
    {
        MemoryPoolSetEntry * entry = CONTAINER_OF(i, MemoryPoolSetEntry, header);
        i = LibosTreeSuccessor(i);
        
        if (LibosOk == KernelMemoryPoolAllocateContiguousInternal(entry->node, ContiguousAllocateAllocateAfter, 0, size, physicalAddress, 1))
            return LibosOk;
    }

    return LibosErrorOutOfMemory;
}

/**
 * @brief Allocates a specific physical address from the pool.
 * 
 * This is intended to be used to grow the size of contiguous allocations.
 * 
 * @param memorySet 
 * @param physicalAddress - Must be a multiple of the pageSize
 * @param size 
 * @return LibosStatus 
 */
LibosStatus KernelNumaAllocateContiguousAt(MemorySet * memorySet, LwU64 physicalAddress, LwU64 size)
{
    size = LibosRoundUp(size, LIBOS_CONFIG_PAGESIZE);

    if (LibosRoundDown(physicalAddress, LIBOS_CONFIG_PAGESIZE) != physicalAddress)
        return LibosErrorFailed;

    LibosTreeHeader * i = LibosTreeLeftmost(LibosTreeRoot(memorySet));
    while (!i->isNil) 
    {
        MemoryPoolSetEntry * entry = CONTAINER_OF(i, MemoryPoolSetEntry, header);
        i = LibosTreeSuccessor(i);
                    
        return KernelMemoryPoolAllocateContiguousInternal(entry->node, ContiguousAllocateExact, physicalAddress, size, 0, 1);
    }

    return LibosErrorOutOfMemory;
}

static void KernelMemoryPoolDestroy(MemoryPool * node)
{
    // We don't have MemoryPool offlining, this should never happen
    KernelPanilwnless(0);
}

/**
 * @brief Initialize the MemoryPool subsystem
 * 
 * We must bootstrap our object pools since we're the source of their physical memory.
 */
void KernelInitMemoryPool()
{
    // Create the pool
    KernelPoolConstruct(sizeof(MemoryPool), LIBOS_POOL_MEMORY_POOL, (void (*)(void *))KernelMemoryPoolDestroy);    
    
    // Add a bootstrap page to the pool since this object is created
    // before the first MemoryPool is registered to the slab allocator
    static __attribute__((aligned(LIBOS_CONFIG_PAGESIZE))) LwU8 bootstrapPage[LIBOS_CONFIG_PAGESIZE];
    KernelPoolBootstrap(LIBOS_POOL_MEMORY_POOL, &bootstrapPage[0], sizeof(bootstrapPage));

    // Initialize the binary tree of physical memory nodes
    LibosTreeConstruct(&memoryPoolsByAddress);
}

/**
 * @brief Ensures an address range is well formed.
 * 
 * This is used in the method contracts to ensure
 * integer overflow won't happen while traversing 
 * the range.
 * 
 * @param begin 
 * @param size 
 * @return LwBool 
 */
LwBool KernelRangeDoesntWrap(LwU64 begin, LwU64 size)
{
    if ((begin + size) < begin)
        return LW_FALSE;
    return LW_TRUE;
}

/**
 * @brief Ensures an address or size is page aligned.
 * 
 * @param page 
 * @return LwBool 
 */
LwBool KernelAddressPageAligned(LwU64 page)
{
    return (page & (LIBOS_CONFIG_PAGESIZE - 1)) == 0;
}