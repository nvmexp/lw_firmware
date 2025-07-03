/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include "libos.h"  // CONTAINER_OF
#include "diagnostics.h"
#include "address_space.h"
#include "libriscv.h"
#include "objectpool.h"   
#include "scheduler.h"
#include "task.h"
#include "mm/memorypool.h"
#include "mm/objectpool.h"
#include "softmmu.h"
#include "pin.h"
#include "libbit.h"

extern PageTableGlobal kernelGlobalPagetable;
AddressSpace       * kernelAddressSpace;
AddressSpaceRegion * kernelTextMapping, *kernelDataMapping, *kernelPrivMapping, *kernelGlobalPageMapping;

AddressSpaceRegion * KernelPhysicalIdentityMapSlots[8];

static void KernelAddressSpacePrintInternal(LibosTreeHeader * node, int depth);

static void KernelAddressSpaceDestroy(AddressSpace * region);

static AddressSpaceRegion * KernelAddressSpaceProbe(BORROWED_PTR AddressSpace * allocator, LwU64 probe)
{
    LibosTreeHeader * node = LibosTreeRoot(&allocator->header);

    while (!node->isNil)
    {
        AddressSpaceRegion * region = CONTAINER_OF(node, AddressSpaceRegion, header);
        if (probe < region->start)
            node = node->left;
        else if (probe >= region->end)
            node = node->right;
        else {
            KernelTrace(" va probe: %llx -> [%llx, %llx)\n", probe, region->start, region->end);
            return region;    
        }
    }

    KernelTrace(" va probe: %llx -> ()\n", probe);
    return 0;
}

static void KernelAddressSpaceRegionMutator(LibosTreeHeader * node)
{
    AddressSpaceRegion * i = CONTAINER_OF(node, AddressSpaceRegion, header);
    
    // Free space in this node
    if (!i->allocated)
        i->largestFreeSize = i->end - i->start;
    else
        i->largestFreeSize = 0;

    // Free space in left subtree
    if (!i->header.left->isNil)
    {
        AddressSpaceRegion * l = CONTAINER_OF(i->header.left, AddressSpaceRegion, header);
        i->largestFreeSize = LW_MAX(i->largestFreeSize, l->largestFreeSize);
    }

    // Free space in right subtree
    if (!i->header.right->isNil)
    {
        AddressSpaceRegion * r = CONTAINER_OF(i->header.right, AddressSpaceRegion, header);
        i->largestFreeSize = LW_MAX(i->largestFreeSize, r->largestFreeSize);
    }
}
            
static void KernelAddressSpaceInsert(BORROWED_PTR AddressSpace * allocator, OWNED_PTR AddressSpaceRegion * node)
{
    LibosTreeHeader * header = &allocator->header;
    LibosTreeHeader * * slot = &header->parent;
    node->header.parent = node->header.left = node->header.right = header;
    while (!(*slot)->isNil)
    {
        node->header.parent = *slot;
        if (CONTAINER_OF(node, AddressSpaceRegion, header)->start < CONTAINER_OF(*slot, AddressSpaceRegion, header)->start)
            slot = &(*slot)->left;
        else
            slot = &(*slot)->right;
    }
    *slot = &node->header;

    LibosTreeInsertFixup(&node->header, KernelAddressSpaceRegionMutator);
}

static AddressSpaceRegion * KernelAddressSpaceSplit(BORROWED_PTR AddressSpace * allocator, BORROWED_PTR AddressSpaceRegion * p, LwU64 probeAddress, LwU64 size)
{
    AddressSpaceRegion * left = 0;
    AddressSpaceRegion * right = 0;

    KernelTrace(" va split: [%llx, %llx) alloc:%d  subregion: [%llx, %llx)\n", p->start, p->end, p->allocated, probeAddress, probeAddress+size);

    if (!p->allocated && 
        probeAddress + size >= probeAddress && // no overflow
        probeAddress >= p->start &&
        (probeAddress + size) <= p->end)
    {
        KernelTrace("Splitting block %llx-%llx for probe %llx-%llx\n", p->start, p->end, probeAddress, probeAddress+size);
        
        // Mark current node as allocated.  This is a user allocation and will hold a reference to the parent address space object 
        // @bug: Huge bug, in the event we cannot allocate new region nodes, we do not cleanly unwind this operation
        p->allocated = LW_TRUE;
        p->parent = allocator;
        KernelPoolAddref(allocator);
        p->largestFreeSize = 0;

        // Free space to the left
        LwU64 leftSpace = probeAddress - p->start;
        if (leftSpace) {
            left = (AddressSpaceRegion *)KernelPoolAllocate(LIBOS_POOL_ADDRESS_SPACE_REGION);
            if (!left)
                goto cleanup;
            left->allocated = LW_FALSE;
            left->parent = 0;
            left->start = p->start;
            left->end   = p->start + leftSpace;
            left->largestFreeSize = leftSpace;

            // Truncate the node
            p->start = probeAddress;

            // Insert new freespace
            KernelAddressSpaceInsert(allocator, left);            
        }

        // Free space to the right
        LwU64 rightSpace = p->end - probeAddress - size;
        if (rightSpace)
        {
            right = (AddressSpaceRegion *)KernelPoolAllocate(LIBOS_POOL_ADDRESS_SPACE_REGION);
            if (!right) 
                goto cleanup;
            right->allocated = LW_FALSE;
            right->parent = 0;
            right->start = p->end - rightSpace;
            right->end   = p->end;
            right->largestFreeSize = rightSpace;            

            // Truncate the node
            p->end = probeAddress + size;

            // Insert new freespace
            KernelAddressSpaceInsert(allocator, right);
        }

        return p;
    }    

cleanup:
    if (left)
        KernelPoolRelease(left);
    if (right)
        KernelPoolRelease(right);
    return 0;
}

static AddressSpaceRegion * KernelAddressSpaceFirstMatch(AddressSpace * allocator, BORROWED_PTR AddressSpaceRegion * p, LwU64 size)
{
    AddressSpaceRegion * c;

    KernelTrace("  va first  size:%llx\n", size);

    // Is there a solution in this subtree?
    if (p->largestFreeSize < size) 
        return 0;

    // Descend leftward finding the deepest node that will work
    do
    {
        // Is the a solution in the left subtree? Traverse down
        if (!p->header.left->isNil)
        {
            AddressSpaceRegion * l = CONTAINER_OF(p->header.left, AddressSpaceRegion, header);

            // Does the left subtree have a hit?
            if (l->largestFreeSize >= size)
            {
                p = l;
                continue;
            }
        }            

        // Does the node itself have space?
        if ((c = KernelAddressSpaceSplit(allocator, p, p->start, size)))
            return c;

        // By construction the right subtree must have a hit
        KernelAssert(!p->header.right->isNil);
        AddressSpaceRegion * r = CONTAINER_OF(p->header.right, AddressSpaceRegion, header);
        p = r;        
    }
    while (LW_TRUE);
}

static void KernelAddressSpaceRegionDestroy(AddressSpaceRegion * region)
{
    KernelTrace(" Destroy: Region [%llx-%llx] allocated:%d\n", region->start, region->end, region->allocated);
    
    LibosTreeUnlink(&region->header, KernelAddressSpaceRegionMutator);
    
    if (!region->parent)
        return;

    KernelAssert(region->allocated);

    // Clear the pagetables associated with this region
    KernelPagetableClearRange(region->parent->pagetable, region->start, region->end-region->start);

    // Release the reference count on our parent
    KernelPoolRelease(region->parent);
}

static BORROWED_PTR AddressSpaceRegion * KernelAddressSpaceAllocateNext(BORROWED_PTR AddressSpace * allocator, LwU64 probeAddress, LwU64 size)
{
    AddressSpaceRegion * c;

    KernelTrace(" va allocate search:%llx  size:%llx\n", probeAddress, size);

    // Find the search address
    AddressSpaceRegion * i = KernelAddressSpaceProbe(allocator, probeAddress);
    if (!i) 
        return KernelAddressSpaceFirstMatch(allocator, CONTAINER_OF(LibosTreeRoot(&allocator->header), AddressSpaceRegion, header), size);       

    // Is there space directly in the node we hit?
    if ((c = KernelAddressSpaceSplit(allocator, i, probeAddress, size)))
        return c;

    // Move towards the root until we're on the left subtree
    if (!i->header.parent->isNil)
        while (!i->header.parent->isNil && i->header.parent->right == &i->header)
            i = CONTAINER_OF(i->header.parent, AddressSpaceRegion, header);

    // Walk to parent
    if (i->header.parent->isNil)
        return KernelAddressSpaceFirstMatch(allocator, CONTAINER_OF(i->header.right, AddressSpaceRegion, header), size);           
    i = CONTAINER_OF(i->header.parent, AddressSpaceRegion, header);

    // Is there space directly in this node?
    if ((c = KernelAddressSpaceSplit(allocator, i, probeAddress, size)))
        return c;

    // Find the first solution in the right subtree (if it exists)
    if (!i->header.right->isNil) 
        return KernelAddressSpaceFirstMatch(allocator, CONTAINER_OF(i->header.right, AddressSpaceRegion, header), size);

    // Fall back to first match
    return KernelAddressSpaceFirstMatch(allocator, CONTAINER_OF(LibosTreeRoot(&allocator->header), AddressSpaceRegion, header), size);        ;
}


void KernelAddressSpaceFree(BORROWED_PTR AddressSpace * asid, OWNED_PTR AddressSpaceRegion * allocation)
{
    // Mark region as "free"
    allocation->allocated = LW_FALSE;

    // Release any reference count this region by be holding on the parent object
    if (allocation->parent)
        KernelPoolRelease(allocation->parent);        
    allocation->parent = 0;

    // Update the free space tracking structures to root
    do {
        KernelAddressSpaceRegionMutator(&allocation->header);
        allocation = CONTAINER_OF(allocation->header.parent, AddressSpaceRegion, header);
    }
    while (&allocation->header != &asid->header);

    // @note: DO NOT reclaim the node as it is lwrrently linked in.
    // we should be doing coalescing here.
}

static void KernelAddressSpaceDestroy(AddressSpace * addressSpace)
{
    KernelTrace(" Destroy: AddressSpace\n");

    // Erase nodes
    do
    {
        LibosTreeHeader * header = LibosTreeRoot(&addressSpace->header);
        if (header == &addressSpace->header)
            break;

        AddressSpaceRegion * root = CONTAINER_OF(header, AddressSpaceRegion, header);
        
        // Unlink the node
        LibosTreeUnlink(&root->header, KernelAddressSpaceRegionMutator);

        // Ensure this node represented free space
        KernelAssert(!root->parent || root->parent == addressSpace);
        KernelAssert(!root->allocated);
        KernelAssert(KernelPoolReferenceCount(root) == 1);

        // Release the node
        KernelPoolRelease(root);
    } while(1);
    
    // Tear down the address space page tables
    KernelPagetableDestroy(addressSpace->pagetable);

    // Ensure that we have no more ELF mappings 
    KernelAssert(LibosTreeRoot(&addressSpace->mappedElfRegions) == &addressSpace->mappedElfRegions);
}

OWNED_PTR AddressSpaceRegion * KernelAddressSpaceAllocate(BORROWED_PTR AddressSpace * allocator, LwU64 probeAddress, LwU64 size)
{
    // Exact allocation?
    if (probeAddress)
    {
        // Find the search address
        AddressSpaceRegion * i = KernelAddressSpaceProbe(allocator, probeAddress);
        if (!i) 
            return KernelAddressSpaceFirstMatch(allocator, CONTAINER_OF(LibosTreeRoot(&allocator->header), AddressSpaceRegion, header), size);       

        // Is there space directly in the node we hit?
        AddressSpaceRegion * c;
        if ((c = KernelAddressSpaceSplit(allocator, i, probeAddress, size))) 
            return c;

        return 0;
    }

    // Allocate starting at previous search point
    AddressSpaceRegion * r = KernelAddressSpaceAllocateNext(allocator, allocator->lastAllocationOrFree, size);

    // Update wrapping search
    if (r)
        allocator->lastAllocationOrFree = r->end;

    // Ensure the reference count is exactly 1 (as this is a new node)
    KernelAssert(KernelPoolReferenceCount(r) == 1);

    return r;
}


LibosStatus KernelAllocateAddressSpace(LwBool userspace, OWNED_PTR AddressSpace * * outAllocator)
{
    AddressSpace * allocator = (AddressSpace *)KernelPoolAllocate(LIBOS_POOL_ADDRESS_SPACE);
    
    if (!allocator)
        return LibosErrorOutOfMemory;

    // Create the empty tree
    LibosTreeConstruct(&allocator->header);

    // Create the loader mapped sections list
    LibosTreeConstruct(&allocator->mappedElfRegions);    

    // Create the pagetables for userspace
    if (userspace) 
    {
        allocator->pagetable = KernelPagetableAllocateGlobal();
        if (!allocator->pagetable)
        {
            KernelPoolRelease(allocator);
            return LibosErrorOutOfMemory;
        }

        // Register the user-space allocatable region
        LibosStatus status = KernelAddressSpaceRegister(allocator, 0x0, 1ULL << (LIBOS_CONFIG_VIRTUAL_ADDRESS_SIZE - 1));
        if (status != LibosOk)
        {
            KernelPoolRelease(allocator);
            return LibosErrorOutOfMemory;
        }
    }
    
    *outAllocator = allocator;

    return LibosOk;
}

LibosStatus KernelAddressSpaceRegister(BORROWED_PTR AddressSpace * allocator, LwU64 vaStart, LwU64 vaSize)
{
    //KernelPrintf("KernelAddressSpaceRegister %llx %llx\n", vaStart, vaSize);
    // Check for overflow
    KernelPanilwnless(KernelRangeDoesntWrap(vaStart, vaSize) && 
                      KernelPagetableValidAddress(vaStart) && KernelPagetableValidAddress(vaStart + vaSize - 1));

    // Allocate the tracking node
    AddressSpaceRegion * region = (AddressSpaceRegion *)KernelPoolAllocate(LIBOS_POOL_ADDRESS_SPACE_REGION);
    if (!region)
        return LibosErrorOutOfMemory;

    region->header.parent = region->header.left = region->header.right = &allocator->header;
    region->header.isNil = LW_FALSE;

    region->allocated = LW_FALSE;
    region->start = vaStart;
    region->end = vaStart + vaSize;     // overflow check above
    region->largestFreeSize = vaSize;

    // @note: Transfer ownership to the allocator
    KernelAddressSpaceInsert(allocator, region);

    return LibosOk;
}

static void KernelAddressSpacePrintInternal(LibosTreeHeader * node, int depth)
{
#if LIBOS_CONFIG_KERNEL_LOG //otherwise don't even bother
    if (node->isNil)
        return;
    AddressSpaceRegion * self = CONTAINER_OF(node, AddressSpaceRegion, header);
    KernelPrintf("Libos: ");
    for (int i = 0; i < depth; i++)
        KernelPrintf("  ");
    KernelPrintf("[%llx, %llx) alloc:%d subtree-largest-free:%llx\n", self->start, self->end, self->allocated, self->largestFreeSize);
    KernelAddressSpacePrintInternal(node->left, depth+1);
    KernelAddressSpacePrintInternal(node->right, depth+1);
#endif
}

void KernelAddressSpacePrint(AddressSpace * alloc)
{
    KernelAddressSpacePrintInternal(alloc->header.parent, 0);
}

void KernelInitAddressSpace()
{
    // The address space objects must be allocated from a pool composed
    // of 4kb frames.  The kernel maps larger frames using mmap (which in turn
    // depends on this va allocator)
    KernelPoolConstruct(sizeof(AddressSpaceRegion), LIBOS_POOL_ADDRESS_SPACE_REGION, (void (*)(void *))KernelAddressSpaceRegionDestroy);
    KernelPoolConstruct(sizeof(AddressSpace), LIBOS_POOL_ADDRESS_SPACE, (void (*)(void *))KernelAddressSpaceDestroy);

    // Create the global kernel address space
    KernelPanilwnless(LibosOk == KernelAllocateAddressSpace(LW_FALSE, &kernelAddressSpace));
    
    kernelAddressSpace->pagetable = &kernelGlobalPagetable;

    // Register usable kernel address space. 
    KernelPanilwnless(LibosOk == KernelAddressSpaceRegister(kernelAddressSpace, 0xffffffff00000000ULL, 0x100000000ULL - 0x1000ULL));

    // Reserve the static apertures
    // SoftTLB has this range
    KernelPanilwnless(kernelTextMapping = KernelAddressSpaceAllocate(kernelAddressSpace, LIBOS_CONFIG_KERNEL_TEXT_BASE, LIBOS_CONFIG_KERNEL_TEXT_RANGE));
    KernelPanilwnless(kernelDataMapping = KernelAddressSpaceAllocate(kernelAddressSpace, LIBOS_CONFIG_KERNEL_DATA_BASE, LIBOS_CONFIG_KERNEL_DATA_RANGE));
    KernelPanilwnless(kernelPrivMapping = KernelAddressSpaceAllocate(kernelAddressSpace, LIBOS_CONFIG_PRI_MAPPING_BASE, LIBOS_CONFIG_PRI_MAPPING_RANGE));
    KernelPanilwnless(kernelGlobalPageMapping = KernelAddressSpaceAllocate(kernelAddressSpace, LIBOS_CONFIG_GLOBAL_PAGE_MAPPING_BASE, LIBOS_CONFIG_GLOBAL_PAGE_MAPPING_RANGE));

    // Create 16mb mapping buffers for KernelMapUser
    for (LwU64 i = 0; i < sizeof(KernelPhysicalIdentityMapSlots)/sizeof(*KernelPhysicalIdentityMapSlots); i++)      
        KernelPhysicalIdentityMapSlots[i] = KernelAddressSpaceAllocate(kernelAddressSpace, 0, 0x1000000);  
}

LibosStatus KernelAddressSpaceMapContiguous(BORROWED_PTR AddressSpace * addressSpace, BORROWED_PTR AddressSpaceRegion * region, LwU64 offset, LwU64 physicalBase, LwU64 size, LwU64 access)
{
    // @todo: Use __builtin_add for overflow
    if (region->start + size > region->end)
        return LibosErrorArgument;

    KernelAssert(KernelRangeDoesntWrap(region->start, size) &&
                 KernelAddressPageAligned(region->start) &&
                 KernelAddressPageAligned(size));

    // Setup kernel MMIO mapping
    for (LwU32 i = 0; i < size; i+=4096)
    {
        
        LibosStatus status = 
            KernelPagetableMapPage(addressSpace->pagetable, 
                                    region->start+offset+i, 
                                    physicalBase+i, 
                                    4096,
                                    access);

        // @todo: We should unmap on failure                            
        if (status != LibosOk)
            return status;
    }

    // @todo: This API requires page aligned virtualAddress and size
    //        Ensure that's in the contract for this function.
    KernelSoftmmuFlushPages(region->start + offset, size);
    return LibosOk;
}

void * KernelMapPhysicalAligned(LwU64 slotNumber, LwU64 physicalAddress, LwU64 size)
{
    KernelAssert(KernelRangeDoesntWrap(physicalAddress, size) &&
                 KernelAddressPageAligned(physicalAddress) &&
                 KernelAddressPageAligned(size));

    // Validate the slot, these are constants in the source
    KernelAssert(slotNumber < sizeof(KernelPhysicalIdentityMapSlots)/sizeof(*KernelPhysicalIdentityMapSlots));

    // Validate the slot is large enough
    if (size >= KernelPhysicalIdentityMapSlots[slotNumber]->end)
        return 0;    

    // Map the pages
    for (LwU32 i = 0; i < size; i+=4096)
    {
        if (LibosOk != KernelPagetableMapPage(
            &kernelGlobalPagetable, 
            KernelPhysicalIdentityMapSlots[slotNumber]->start + i, 
            physicalAddress + i, 
            4096,
            LibosMemoryReadable | LibosMemoryWriteable))
            return 0;
    }

    // @todo: This API requires page aligned virtualAddress and size
    //        Ensure that's in the contract for this function.
    KernelSoftmmuFlushPages(KernelPhysicalIdentityMapSlots[slotNumber]->start, size);

    return (void *)KernelPhysicalIdentityMapSlots[slotNumber]->start;
}

void * KernelMapUser(LwU64 slotNumber, Task * self, LwU64 address, LwU64 size)
{
    // Is this a well formed range?
    if (!size || !KernelRangeDoesntWrap(address, size))
        return 0;

    // Validate the slot, these are constants in the source
    KernelAssert(slotNumber < sizeof(KernelPhysicalIdentityMapSlots)/sizeof(*KernelPhysicalIdentityMapSlots));

    // Compute the mapped pointer address
    LwU64 slotBase = KernelPhysicalIdentityMapSlots[slotNumber]->start;
    void * mapping = (void *)(slotBase + (address&(LIBOS_CONFIG_PAGESIZE-1)));

    // Compute the start of the mapping in user-space
    LwU64 userAddress = LibosRoundDown(address, LIBOS_CONFIG_PAGESIZE);
    LwU64 userSize = LibosRoundUp((address + size - 1) - userAddress, LIBOS_CONFIG_PAGESIZE);

    // Validate the slot is large enough
    if (userSize >= KernelPhysicalIdentityMapSlots[slotNumber]->end)
        return 0;
    
    // Copy the pagetable entries. We do not need to clean up
    // if this operation fails, the next mapping will do that.
    if (LibosOk != KernelPagetablePinAndMap(&kernelGlobalPagetable, 
                            slotBase, 
                            self->asid->pagetable, 
                            userAddress, 
                            userSize))
        return 0;

    return mapping;
}

const char * KernelMapUserString(LwU64 slotNumber, Task * self, LwU64 address, LwU64 length)
{
    const char * mapping = (const char *)KernelMapUser(slotNumber, self, address, length);
    
    if (!mapping)
        return 0;

    // Validate the presence of the null pointer
    if (mapping[length])
        return 0;

    return mapping;
}

void KernelUnmapUser(LwU64 slotNumber, LwU64 size) 
{
    KernelPagetableClearRange(&kernelGlobalPagetable, 
                              KernelPhysicalIdentityMapSlots[slotNumber]->start,
                              LibosRoundUp(size, LIBOS_CONFIG_PAGESIZE));
}

/*!
 *
 *  Reserved 4mb of virtual address space
 *     NewRegion = LibosAddressSpaceMMap(LibosDefaultAddressSpace, 0, 0, 4096*1024, LibosMemoryReadable | LibosMemoryWriteable, MMAP_RESERVE, 0, 0);
 *
 *  Allocating 4mb of anonymous page(s)
 *     NewRegion = LibosAddressSpaceMMap(LibosDefaultAddressSpace, 0, 0, 4096*1024, LibosMemoryReadable | LibosMemoryWriteable, MMAP_POPULATE, 0, 0);
 * 
 *   Mapping the first 4kb of AddressSpaceRegion provided by another task
 *     NewRegion = LibosAddressSpaceMMap(LibosDefaultAddressSpace, 0, 0, 4096, LibosMemoryReadable | LibosMemoryWriteable, MMAP_SHARED, remoteRegion, 0);
 * 
 *   Allocate 4k of anonymous pages at a specific address
 * 
 * @todo:
 *    Do we really need to expose AddresSpaceRegion to user-space?
 *       Allows remapping without pain.
 *    Could we not add a virtual address specific API to attach to messages?
 *    Then allowing virtual addresses to be used and a mostly conformant MMAP to be implemented? 
 *    Can we map a port?
 */ 
LibosStatus KernelAddressSpaceMap(
    AddressSpace       *   addressSpace, 
    AddressSpaceRegion * * region,         
    LwU64                * offset,
    LwU64                  size,
    LwU64                  protection,
    LwU64                  flags,
    MemorySet * memorySet)      
{
    // Validate the arguments
    if (flags &~ (LibosMemoryFlagUnmap | LibosMemoryFlagPopulate))
        return LibosErrorArgument;

    // Allocate a virtual memory region if none is present
    if (!*region) 
    {
        AddressSpaceRegion * newRegion = KernelAddressSpaceAllocate(addressSpace, *offset, size);
        if (!newRegion)
            return LibosErrorOutOfMemory;
        *offset = newRegion->start;
        *region = newRegion;
    }

    if (flags & LibosMemoryFlagPopulate)
    {
        // Must provide a physical memory node
        if (!memorySet)
            return LibosErrorArgument;

        // Loop over the pages in the region assigning and mapping backing store
        for (LwU64 offset = 0; offset < size; offset+=LIBOS_CONFIG_PAGESIZE)
        {
            LwU64 page;
            LibosStatus status = KernelMemoryPoolAllocatePage(memorySet, LIBOS_CONFIG_PAGESIZE_LOG2, &page); 
            KernelPanilwnless(status == LibosOk);
            
            // Map contiguous will bump the reference count
            status = KernelAddressSpaceMapContiguous(addressSpace, *region, offset, page, LIBOS_CONFIG_PAGESIZE, protection);
            KernelPanilwnless(status == LibosOk);
            KernelMemoryPoolRelease(page, LIBOS_CONFIG_PAGESIZE);
        }
    }
    else if (flags & LibosMemoryFlagUnmap)
    {
        KernelPanilwnless(0);
    }

    return LibosOk;
}

__attribute__((used)) LibosStatus KernelSyscallRegionMapGeneric(
    LibosAddressSpaceHandle      /* a0 */ addressSpaceHandle, 
    LibosRegionHandle            /* a1 */ regionHandle,         
    LwU64                        /* a2 */ offset,
    LwU64                        /* a3 */ size,
    LwU64                        /* a4 */ access,
    LwU64                        /* a5 */ flags,
    LibosMemorySetHandle /* a6 */ memorySetHandle
)
{
    Task * task = KernelSchedulerGetTask();
    AddressSpace       * addressSpace = KernelObjectResolve(KernelSchedulerGetTask(), addressSpaceHandle, AddressSpace, LibosGrantAddressSpaceAll);
    AddressSpaceRegion * region       = KernelObjectResolve(KernelSchedulerGetTask(), regionHandle, AddressSpaceRegion, LibosGrantRegionAll);
    MemorySet  * memorySet    = KernelObjectResolve(KernelSchedulerGetTask(), memorySetHandle, MemorySet, LibosGrantMemorySetAll);
    LwBool passedARegion = !!region;
    LibosStatus status = KernelAddressSpaceMap(
        addressSpace, 
        &region,
        &offset,
        size,
        access,
        flags,
        memorySet
    );

    if (!passedARegion) {
        LibosStatus status2 = KernelTaskRegisterObject(task, &regionHandle, region, LibosGrantRegionAll, 0);
        if (status2 != LibosOk)
            status = status2;
        task->state.registers[RISCV_A1] = regionHandle;
    }

    task->state.registers[RISCV_A2] = offset;

    KernelPoolRelease(addressSpace);
    KernelPoolRelease(region);
    KernelPoolRelease(memorySet);
    
    return status;
}