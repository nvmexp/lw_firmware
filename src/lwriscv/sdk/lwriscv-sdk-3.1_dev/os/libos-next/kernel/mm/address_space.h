/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#pragma once 

#include <lwtypes.h>
#include "librbtree.h"
#include "libos_status.h"
#include "pagetable.h"
#include "../loader.h"
#include "mm/objectpool.h"
#include "mm/memorypool.h"

/**
 *  @brief Tracks a reservation of a range of virtual addresses.
 *        
 */
typedef struct {
    LibosTreeHeader                   header;
    OWNED_PTR   struct AddressSpace * parent;
    LwBool                            allocated;
    LwU64                             start, end;         // [start, end]
    LwU64                             largestFreeSize;    // largest VA block free in this subtree
} AddressSpaceRegion;

typedef struct AddressSpace {
    LibosTreeHeader   header;
    PageTableGlobal * pagetable;
    LibosTreeHeader   mappedElfRegions;
    LwU64             lastAllocationOrFree;
} AddressSpace;

// Constructs an empty address space object
LibosStatus KernelAllocateAddressSpace(LwBool userSpace, OWNED_PTR AddressSpace * * allocator);

// Registers a region of free virtual memory with the allocator
LibosStatus KernelAddressSpaceRegister(BORROWED_PTR AddressSpace * allocator, LwU64 vaStart, LwU64 vaSize);

OWNED_PTR AddressSpaceRegion * KernelAddressSpaceAllocate(BORROWED_PTR AddressSpace * allocator, LwU64 probeAddress, LwU64 size);
void KernelAddressSpaceFree(BORROWED_PTR AddressSpace * asid, OWNED_PTR AddressSpaceRegion * allocation);
void KernelAddressSpacePrint(BORROWED_PTR AddressSpace * alloc);

void KernelInitAddressSpace();

// Kernel address space and bootstrapped mappings
extern AddressSpace       * kernelAddressSpace;
extern AddressSpaceRegion * kernelTextMapping, *kernelDataMapping, *kernelPrivMapping, *kernelGlobalPageMapping;

// virtualAddress may be null
LibosStatus KernelAddressSpaceMapContiguous(BORROWED_PTR AddressSpace * space, BORROWED_PTR AddressSpaceRegion * addressSpace, LwU64 offset, LwU64 physicalBase, LwU64 size, LwU64 flags);

void * KernelMapPhysicalAligned(LwU64 slotNumber, LwU64 physicalAddress, LwU64 size);
void * KernelMapUser(LwU64 slotNumber, Task * task, LwU64 address, LwU64 size);
const char * KernelMapUserString(LwU64 slotNumber, Task * task, LwU64 address, LwU64 length);
void KernelUnmapUser(LwU64 slotNumber, LwU64 size);

LibosStatus KernelAddressSpaceMap(
    AddressSpace       *   addressSpace, 
    AddressSpaceRegion * * region,         
    LwU64                * offset,
    LwU64                  size,
    LwU64                  protection,
    LwU64                  flags,
    MemorySet * memorySet);