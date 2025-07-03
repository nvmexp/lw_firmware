#pragma once
#include <lwtypes.h>
#include <libos.h>
#include "librbtree.h"
#include "memoryset.h"
#include "pagestate.h"
#include "mm/objectpool.h"

typedef struct MemoryPool {
    LibosTreeHeader      header;
    LwU64                physicalBase;  // [physicalBase, physicalLast]
    LwU64                physicalLast; 

    // Internal state for numa.c
    struct Buddy    *    buddyTree;
    PageState       *    pageState;
} MemoryPool;

void        KernelInitMemoryPool();
void        KernelInitMemorySet();
LibosStatus KernelMemoryPoolAllocatePage(MemorySet * memorySet, LwU64 pageLog2, LwU64 * physicalAddress);

LibosStatus KernelNumaAllocateContiguous(MemorySet * memorySet, LwU64 size, LwU64 * physicalAddress);
LibosStatus KernelNumaAllocateContiguousAt(MemorySet * memorySet, LwU64 physicalAddress, LwU64 size);

LibosStatus KernelAllocateMemoryPool(LwU64 physicalBase, LwU64 physicalLast, MemoryPool * * physicalMemoryNode);

MemoryPool * KernelMemoryPoolFind(LwU64 address);

LwBool       KernelRangeDoesntWrap(LwU64 begin, LwU64 size);
LwBool       KernelAddressPageAligned(LwU64 page);