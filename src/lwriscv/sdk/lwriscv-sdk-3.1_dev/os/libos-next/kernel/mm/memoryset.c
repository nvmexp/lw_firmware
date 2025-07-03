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
 * @brief Destructor for a memory set.  
 * 
 * @param set 
 */
void KernelMemorySetDestroy(MemorySet * set) 
{
    // Remove any nodes in this tree
    while (!set->parent->isNil) {
        MemoryPoolSetEntry * entry = CONTAINER_OF(set->parent, MemoryPoolSetEntry, header);
        LibosTreeUnlink(&entry->header, 0);
        KernelPoolRelease(entry->node);
        KernelPoolRelease(entry);
    }
}

/**
 * @brief Initial the memory set subsystem.
 * 
 *        Sets up pool allocations for MemoryPoolSetEntry and MemorySet
 */
void KernelInitMemorySet()
{
    // Create the pool
    KernelPoolConstruct(sizeof(MemoryPoolSetEntry), LIBOS_POOL_MEMORY_SET_ENTRY, 0);
    KernelPoolConstruct(sizeof(MemorySet), LIBOS_POOL_MEMORY_SET, (void (*)(void *))KernelMemorySetDestroy);

    // Bootstrap the pool, since it comes up before the first memory set is registered
    static __attribute__((aligned(4096))) LwU8 bootstrapPage1[4096];
    KernelPoolBootstrap(LIBOS_POOL_MEMORY_SET_ENTRY, &bootstrapPage1[0], sizeof(bootstrapPage1));

    // Bootstrap the pool, since it comes up before the first memory set is registered
    static __attribute__((aligned(4096))) LwU8 bootstrapPage2[4096];
    KernelPoolBootstrap(LIBOS_POOL_MEMORY_SET, &bootstrapPage2[0], sizeof(bootstrapPage2));
}

/**
 * @brief Allocates a new memory set
 * 
 * @param physicalMemorySet 
 * @return LibosStatus 
 */
LibosStatus KernelMemorySetAllocate(MemorySet * * physicalMemorySet) {
    MemorySet * set = (MemorySet *)KernelPoolAllocate(LIBOS_POOL_MEMORY_SET);
    if (!set)
        return LibosErrorOutOfMemory;
    LibosTreeConstruct(set);
    *physicalMemorySet = set;
    return LibosOk;
}

/**
 * @brief Adds a pool of memory to an existing memory set.
 *        The kernel prefers to allocate pages from MemoryPools with
 *        lower distances
 * 
 * 
 * @param set 
 * @param node 
 * @param distance 
 * @return LibosStatus 
 */
LibosStatus KernelMemorySetInsert(BORROWED_PTR MemorySet * set, OWNED_PTR MemoryPool * node, LwU64 distance) {
    MemoryPoolSetEntry * entry = (MemoryPoolSetEntry *)KernelPoolAllocate(LIBOS_POOL_MEMORY_SET_ENTRY);
    if (!entry)
        return LibosErrorOutOfMemory;
    entry->node = node;
    entry->distance = distance;
    KernelPoolAddref(node);

    LibosTreeHeader * * slot = &set->parent;
    entry->header.parent = entry->header.left = entry->header.right = set;
    
    while (!(*slot)->isNil)
    {
        entry->header.parent = *slot;
        if (entry->distance < CONTAINER_OF(*slot, MemoryPoolSetEntry, header)->distance)
            slot = &(*slot)->left;
        else
            slot = &(*slot)->right;
    }
    *slot = &entry->header;

    LibosTreeInsertFixup(&entry->header, 0);    

    return LibosOk;
}

/**
 * @brief User-space syscall handler for KernelMemorySetAllocate
 * 
 * @return LibosStatus 
 */
LibosStatus KernelSyscallMemorySetAllocate() 
{
    Task * self = KernelSchedulerGetTask();
    MemorySet * set;

    // Allocate the kernel object
    LibosStatus status = KernelMemorySetAllocate(&set);
    if (status != LibosOk)
        return status;

    // Map it to user-space
    LibosMemorySetHandle handle;
    status = KernelTaskRegisterObject(self, &handle, set, LibosGrantMemorySetAll, 0);
    KernelPoolRelease(set);

    // Return the value
    self->state.registers[RISCV_T0] = handle;

    return status;
}

/**
 * @brief User-space syscall handler for KernelMemorySetInsert
 * 
 * @param setHandle 
 * @param nodeHandle 
 * @param distance 
 * @return LibosStatus 
 */
LibosStatus KernelSyscallMemorySetInsert(LibosMemorySetHandle setHandle, LibosMemoryPoolHandle nodeHandle, LwU64 distance) 
{
    MemorySet * set = KernelObjectResolve(KernelSchedulerGetTask(), setHandle, MemorySet, LibosGrantMemorySetAll);
    MemoryPool * node = KernelObjectResolve(KernelSchedulerGetTask(), nodeHandle, MemoryPool, LibosGrantMemoryPoolAll);

    if (!set || !node) {
        KernelPoolRelease(set);
        KernelPoolRelease(node);
    }

    return KernelMemorySetInsert(set, node, distance);
}
