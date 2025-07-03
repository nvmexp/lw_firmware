#pragma once
#include <lwtypes.h>
#include <libos.h>
#include "librbtree.h"
#include "mm/objectpool.h"

typedef struct {
    LibosTreeHeader      header;
    struct MemoryPool  * node;
    LwU64                distance;
} MemoryPoolSetEntry;

typedef LibosTreeHeader MemorySet;

LibosStatus KernelMemorySetAllocate(MemorySet * * physicalMemoryNode);
LibosStatus KernelMemorySetInsert(BORROWED_PTR MemorySet * set, OWNED_PTR struct MemoryPool * node, LwU64 distance);


extern OWNED_PTR MemorySet * KernelPoolMemorySet;
