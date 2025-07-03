#include <lwtypes.h>

typedef struct NumaMemory
{
    struct NumeMemory * next;
    LwU64 regionStart;
    LwU64 entryCount;
    LwU64 freeMap[1];
};

struct NumaMemory * memory = 0;

void KernelSyscallRegisterMemory(LwU64 begin, LwU64 end)
{
    // @todo: Assume we're within the data section of the init task.
    //        Down the road we'll need to update the kernel page tables
    //        and allow paging to populate the entry.
    
    // @todo: Validate the caller has priveleges to make this call
    if (KernelSchedulerGetTask()->id != 0)    
        KernelPanic();

    // Panic if we don't have enough memory to describe the bitmap
    if (end < begin || (end-begin) < sizeof(NumaMemory))
        KernelPanic();

    // Create the node
    struct NumaMemory * node = (NumaMemory *)begin;
    node->next = memory;
    node->regionStart = begin;
    
    // Callwlate entry count
    LwU64 pages = (end-begin+LIBOS_PAGE_SIZE-1)/LIBOS_PAGE_SIZE;
    node->entryCount = (page + 63)/64;

    // Zero the bitmap
    for (LwU64 i = 0; i < node->entryCount; i++)
        node->freeMap[i] = 0;

    // Mark the tracking structure as allocated
    LwU64 allocatedSize = offsetof(NumaMemory, freeMap[node->entryCount]);
    LwU64 pages = (allocatedSize + LIBOS_PAGE_SIZE-1)/LIBOS_PAGE_SIZE;
    for (LwU64 i = 0; i < pages; i++)
        node->freeMap[i/64] |= 1ULL << (i & 63);

    // Link in the new memory node
    memory = node;
}

void KernelSyscallAllocatePages(LwU64 size, LwU64 alignment)
{

}

Slab * KernelSlabCreate(LwU64 elementSize)
{

}

void * KernelSlabAllocate(Slab * slab)
{

}

void KernelSlabFree(Slab * slab, void * element)
{

}