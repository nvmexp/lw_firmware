#include <lwtypes.h>
#include "kernel.h"
#include "libos.h"
#include "libbit.h"
#include "objectpool.h"
#include "memorypool.h"
#include "list.h"
#include "mm/identity.h"
#include "pin.h"

/**
 * @brief Kernel Object Pool
 * 
 * An object pool is a collection of kernel pages called 'slabs'.
 * Each slab is divided up into uniform sized chunks.
 * 
 * Slabs are always the minimum page size supported by the kernel
 * to prevent memory fragmentation issues.
 * 
 * Slabs are mapped through the kernel identity map.
 * 
 */
typedef struct ObjectPool
{
    // Object metadata
    LwU64         objectSize;                   // Includes the reference count header
    void        (*destructor)(void * ptr);

    // Linked list of slabs within this pool that have free pages
    ListHeader    slabsWithSpace;
} ObjectPool;

/**
 * @brief Slab of user objects
 * 
 * The slab header of any object can be located by rounding
 * the address down to the LIBOS_CONFIG_PAGESIZE.
 * 
 * The slab allocator is only appropriate for objects significantly
 * smaller than the page size.
 * 
 * Free objects are stored in a single linked list through the
 * firstFreeInBlock pointer.  The first word of each block
 * points to the next block.
 * 
 * @note: The slab header could be moved out of the slab page
 *        by storing the pointer in the PageState object.
 *        This also has the advantage of allowing the Slab
 *        allocator to use pages larger than LIBOS_CONFIG_PAGE_SIZE
 * 
 * |---------------------|      <-- page is LIBOS_CONFIG_PAGESIZE bytes
 * |   Slab (header)     |
 * |---------------------|
 * |   Object 0          |
 * |---------------------|   
 * |   Object 1          |
 * |---------------------|   
 * |   ...               |
 * |---------------------|   
 * |   Object n          |
 * |---------------------|   
 * 
 * 
 */
typedef struct Slab
{
    LwBool              wasBootstrapped;
    LwU32               allocated;          // How much objects are allocated from this slab
    ListHeader          header;             // Link for the ObjectPool::slabsWithSpace list
    void              * firstFreeBlock;     // Linked list of free objects
    ObjectPool        * parent;             
} Slab;


/**
 * @brief Global list of all constructed ObjectPools
 * 
 * This is done so we can assign a simple numeric-id for
 * each slab.  This slightly increases the code density
 * for DynamicCast code as we're doing a tiny constant
 * load.
 * 
 */
static ObjectPool pools[LIBOS_POOL_COUNT] = {0};


/**
 * @brief Construct a new ObjectPool
 * 
 * Each ObjectPool is assigned a unique PoolId for run time type
 * detection.  
 * 
 * 
 * 
 * @param objectSize 
 * @param poolId     
 * @param pfnDestroy Optional destructor 
 */
void KernelPoolConstruct(LwU64 objectSize, LibosPool poolId, void (*pfnDestroy)(void * ptr))
{
    ListConstruct(&pools[poolId].slabsWithSpace);
    pools[poolId].destructor = pfnDestroy;
    pools[poolId].objectSize = sizeof(LwU64) + objectSize;
}

/**
 * @brief Grows an ObjectPool by adding the page to the pool.
 * 
 * The page must be of size LIBOS_CONFIG_PAGESIZE.
 * 
 * This is internally called both by KernelPoolGrow and KernelPoolBootstrap.
 * 
 * @note: In the case of bootstrapping the page comes from the kernel data
 *        area which is outside of the MemoryPool for the kernel.  Do not
 *        assume PageState exists for ObjectPool.
 * 
 * @param pool 
 * @param page 
 */
static void KernelPoolAddPageInternal(struct ObjectPool * pool, void * page, LwBool wasBootstrapped)
{
    // Slabs must be page aligned
    KernelAssert(LibosRoundDown((LwU64)page, LIBOS_CONFIG_PAGESIZE) == (LwU64)page);

    // Fill out the slab
    struct Slab * slab = (Slab *)page;
    slab->parent = pool;
    slab->allocated = 0;
    slab->firstFreeBlock = slab+1;
    slab->wasBootstrapped = wasBootstrapped;

    // Divide the memory after the header into objectSize blocks
    LwU64 objectCount = (LIBOS_CONFIG_PAGESIZE - sizeof(struct Slab)) / pool->objectSize;
    for (LwU32 i = 0; i < objectCount - 1; i++)
        *(void **)((LwU64)slab->firstFreeBlock + i * pool->objectSize) = 
            (void *)((LwU64)slab->firstFreeBlock + (1+i) * pool->objectSize);
    *(void **)((LwU64)slab->firstFreeBlock + (objectCount - 1) * pool->objectSize) = 0;

    // Link the slab into the list
    ListPushFront(&pool->slabsWithSpace, &slab->header);
}

/**
 * @brief Grows the ObjectPool by a single page
 * 
 * Returns LibosOutOfMemory if we're unable to secure a page from the MemoryPool.
 * 
 * @param pool 
 * @return LibosStatus 
 */
LibosStatus KernelPoolGrow(struct ObjectPool * pool)
{
    LwU64 slabPage;

    // Allocate a new page of the right size
    // @note This function is returning a physical address.
    LibosStatus status = KernelMemoryPoolAllocatePage(KernelPoolMemorySet, LIBOS_CONFIG_PAGESIZE_LOG2, &slabPage);
    if (status != LibosOk)
        return status;

    // Add the page to the pool
    // @note: We use the identity mapping for performance 
    KernelPoolAddPageInternal(pool, KernelPhysicalIdentityMap(slabPage), LW_FALSE /* not a bootstrap page */);

    return LibosOk;
}

/**
 * @brief Bootstrap an ObjectPool with a user-provided page
 * 
 * This is required for to initialize pools that must be used
 * before the MemoryPool/MemorySet system is fully initialized.
 * 
 * @param poolId 
 * @param page 
 * @param size 
 */
void KernelPoolBootstrap(LibosPool poolId, void * page, LwU64 size)
{
    // Must be slab page sized
    KernelPanilwnless(size == (1ULL << LIBOS_CONFIG_PAGESIZE_LOG2));

    // Must be naturally aligned
    KernelPanilwnless(((LwU64)page & ((1ULL << LIBOS_CONFIG_PAGESIZE_LOG2)-1)) == 0);

    KernelPoolAddPageInternal(&pools[poolId], page, LW_TRUE /* Is a bootstrap page */);
}

/**
 * @brief Locates which pool an ObjectPool pointer belongs to
 * 
 * Slabs are always naturally aligned pages of LIBOS_CONFIG_PAGESIZE,
 * rounding down to the page start locates the slab header.
 * 
 * Generally, this routine is used for fast runtime type identification.
 * 
 * @see Slab for diagram. 
 * 
 * @param ptr 
 * @return LibosPool 
 */
LibosPool KernelPoolLocate(void * ptr) 
{
    Slab * slab = (Slab *)(((LwU64)ptr) & ~(LIBOS_CONFIG_PAGESIZE-1));
    return slab->parent - &pools[0];
}

/**
 * @brief Allocates a new object from the pool
 * 
 * The object is guaranteed to zero filled on allocation.
 * 
 * If allocation must be called early in init before MemoryPool is online
 * call KernelPoolBootstrap to provide a page.
 * 
 * All allocations are internally reference counted.  
 * Use KernelPoolAddref/KernelPoolRelease to manage lifetime.
 * Reference count starts at 1. 
 * 
 * All objects have a hidden header storing the reference count.
 * 
 * |-------------------|
 * |  Reference Count  |
 * |-------------------| <- Pointer
 * |      Object       |
 * |-------------------|
 * 
 * @param poolId 
 * @return void* 
 */
void * KernelPoolAllocate(LibosPool poolId)
{
    ObjectPool * pool = &pools[poolId];

    // No slabs with free space? Grow it!
    if (ListEmpty(&pool->slabsWithSpace) && KernelPoolGrow(pool) != LibosOk)
        return 0;

    Slab * slab = CONTAINER_OF(pool->slabsWithSpace.next, Slab, header);

    // Allocate a block from the first free slab
    void * ptr = slab->firstFreeBlock;
    slab->allocated++;
    slab->firstFreeBlock = *(void **)ptr;

    // Got the last block? Remove the slab from the freelist
    if (!slab->firstFreeBlock) 
        ListUnlink(&slab->header);

    // Zero the object    
    memset(ptr, 0, pool->objectSize);

    // Initialize the reference count
    *(LwU64 *)ptr = 1;

    // Return the pointer to the object itself (not counting the header)
    return (LwU64 *)ptr + 1;
}

/**
 * @brief Internal function to return an object to the freelist
 * 
 * @param ptr 
 */
static void   KernelPoolFree(void * ptr)
{
    // Find the true start of the object (the reference count header)
    void * block = ((LwU64 *)ptr) - 1;

    // Find the containing slab
    Slab * slab = (Slab *)(((LwU64)block) & ~(LIBOS_CONFIG_PAGESIZE-1));

    // Update the allocated block count in the slab
    // If the page is empty and wasn't a bootstrap page
    // we'll return it to the system memory allocator
    if (!--slab->allocated && !slab->wasBootstrapped) {
        // Remove the empty slab from the allocator
        ListUnlink(&slab->header);

        // Release the memory
        LwU64 pa = KernelPhysicalIdentityReverseMap(slab);
        KernelMemoryPoolRelease(pa, LIBOS_CONFIG_PAGESIZE);
        return;
    }

    // Not reclaiming the slab? Link the block in
    *(void **)block = slab->firstFreeBlock;
    slab->firstFreeBlock = block;
}

/**
 * @brief Increase the reference count on the allocation.
 * 
 * @param ptr 
 */
void   KernelPoolAddref(void * ptr)
{
    ++*(((LwU64 *)ptr)-1);
}

/**
 * @brief Decrease the reference count and potentially free the object.
 * 
 * @note You may release the reference count on a null object. It has no effect.
 * 
 * @param ptr 
 */
void   KernelPoolRelease(void * ptr)
{
    if (!ptr)
        return;
    if(!--*(((LwU64 *)ptr)-1)) {
        ObjectPool * pool = &pools[KernelPoolLocate(ptr)];
        if (pool->destructor)
            pool->destructor(ptr);
        KernelPoolFree(ptr);
    }
}

/**
 * @brief Returns the current reference count for the object
 *  
 * @param ptr 
 * @return LwU64 
 */
LwU64      KernelPoolReferenceCount(void *ptr)
{
    return *(((LwU64 *)ptr)-1);
}