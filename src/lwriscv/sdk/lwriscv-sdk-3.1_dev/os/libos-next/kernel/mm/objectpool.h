#pragma once

#include <lwtypes.h>
#include "libos.h"
#include "kernel/diagnostics.h"

/*
    Memory Management Strategy
        - BORROWED_PTR variables indicate a variable who isn't directly 
          holding a reference on an object.  Comments should be included
          to show that the source lifetime exceeds our use.
         
        - OWNED_PTR variables indicate a variable who is holding a reference.
          This variable must be passed to an OWNED_PTR function such as
          KernelPoolRelease when done.

    Reference counting rules:
        - You must call KernelPoolAddref before passing a BORROWED_PTR
          value to a function expecting OWNED_PTR
        - You must not leak an OWNED_PTR value. 
        - BORROWED_PTR must have a guarantee that the source lifetime
          exceeds our own.  
        - BORROWED_PTR may exist on the heap only if the object knows
          how to find all pointers to iself on destruction.
            e.g. When freeing a shuttle, we can locate all ports and tasks
                 that may have pointers to this shuttle and remove.  Thus
                 these objects may point to the shuttle using a BORROWED_PTR

    Integrity validation:
        - Simulator will track pointer type and lifetime.
          
*/


/**
 * @brief This variable is borrowing a longer reference.
 * 
 * Do not call KernelPoolRelease when this pointer goes out of scope.
 * 
 */
#define BORROWED_PTR


/**
 * @brief This variable is holding a reference.
 * 
 * Call KernelPoolRelease when variable out of scope.
 * 
 * May be transfered to a function taking an OWNED_PTR 
 * if the source is zeroed.
 * 
 */
#define OWNED_PTR

/**
 * @brief This variable is reachable from the object destructor.
 * 
 * This is a special form of borrowing.
 * 
 */
#define DESTRUCTOR_REMOVES_PTR

typedef enum {
    LIBOS_POOL_PARTITION,
    LIBOS_POOL_ELF,                  
    LIBOS_POOL_MAPPED_ELF,           
    LIBOS_POOL_ADDRESS_SPACE,        
    LIBOS_POOL_TASK,                 
    LIBOS_POOL_PORT,                 
    LIBOS_POOL_SHUTTLE,              
    LIBOS_POOL_TIMER,       
    LIBOS_POOL_ADDRESS_SPACE_REGION, 
    LIBOS_POOL_LOCK,
    LIBOS_POOL_VIRTUAL_MPU_ENTRY,
    LIBOS_POOL_MEMORY_POOL,
    LIBOS_POOL_MEMORY_SET_ENTRY,
    LIBOS_POOL_MEMORY_SET,
    LIBOS_POOL_COUNT                
} LibosPool;

void       KernelPoolConstruct(LwU64 objectSize, LibosPool poolIndex, void (*pfnDestroy)(void * ptr));
void       KernelPoolBootstrap(LibosPool poolId, void * page, LwU64 size);
void *     KernelPoolAllocate(LibosPool poolIndex);
LibosPool  KernelPoolLocate(void * ptr);
LwU64      KernelPoolReferenceCount(void *ptr);

#if !LIBOS_TINY
void       KernelPoolAddref(void * ptr);
void       KernelPoolRelease(void * ptr);
#else
#define    KernelPoolRelease(ptr)
#define    KernelPoolAddref(ptr)
#endif

// Performs a type-safe cast.  Panics if the object is of the wrong type.
// @precondition 'x' should be a pointer from KernelPoolAllocate
#define DynamicCast(T, x)    DynamicCast##T(x)

#define DynamicCastTask(x)                                                                     \
    ({                                                                                         \
        KernelPoolLocate(x) == LIBOS_POOL_TASK ? (Task *)x : 0;                                \
    })

#define DynamicCastPort(x)                                                                     \
    ({                                                                                         \
        LibosPool pool = KernelPoolLocate(x);                                                  \
        (pool == LIBOS_POOL_PORT ||                                                            \
        pool == LIBOS_POOL_PARTITION  ||                                                       \
        pool == LIBOS_POOL_TASK  ||                                                            \
        pool == LIBOS_POOL_TIMER ||                                                            \
        pool == LIBOS_POOL_LOCK )                                                              \
            ? (Port *)x : 0;                                                                   \
    })

#define DynamicCastShuttle(x)                                                                  \
    ({                                                                                         \
        KernelPoolLocate(x) == LIBOS_POOL_SHUTTLE ? (Shuttle *)x : 0;                          \
    })


#define DynamicCastTimer(x)                                                                    \
    ({                                                                                         \
        KernelPoolLocate(x) == LIBOS_POOL_TIMER ? (Timer *)x : 0;                              \
    })

#define DynamicCastAddressSpace(x)                                                             \
    ({                                                                                         \
        KernelPoolLocate(x) == LIBOS_POOL_ADDRESS_SPACE ? (AddressSpace *)x : 0;               \
    })

#define DynamicCastAddressSpaceRegion(x)                                                       \
    ({                                                                                         \
        KernelPoolLocate(x) == LIBOS_POOL_ADDRESS_SPACE_REGION ? (AddressSpaceRegion *)x : 0;  \
    })

#define DynamicCastElf(x)                                                                      \
    ({                                                                                         \
        KernelPoolLocate(x) == LIBOS_POOL_ELF ? (Elf *)x : 0;                                  \
    })

#define DynamicCastPartition(x)                                                                \
    ({                                                                                         \
        KernelPoolLocate(x) == LIBOS_POOL_PARTITION ? (Partition *)x : 0;                      \
    })

#if !LIBOS_TINY
    #define DynamicCastLock(x)                                                                \
        ({                                                                                         \
            KernelPoolLocate(x) == LIBOS_POOL_LOCK ? (Lock *)x : 0;                      \
        })
#else
    /*
        LIBOS_LITE stores all objects in plain arrays
    */
    extern Lock lockArray;
    extern Lock lockArrayEnd;
    #define DynamicCastLock(x)                                                                \
        ({                                                                                         \
            (LwU64)(x) >= (LwU64)&lockArray && (LwU64)(x) < (LwU64)&lockArrayEnd ? (Lock *)x : 0;                      \
        })
#endif

#define DynamicCastMemoryPool(x)                                                               \
    ({                                                                                         \
        KernelPoolLocate(x) == LIBOS_POOL_MEMORY_POOL ? (MemoryPool *)x : 0;                      \
    })

#define DynamicCastMemorySet(x)                                                                \
    ({                                                                                         \
        KernelPoolLocate(x) == LIBOS_POOL_MEMORY_SET ? (MemorySet *)x : 0;                      \
    })

