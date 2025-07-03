/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwosdebug.h"
#include "lwoslayer.h"
#include "lwrtos.h"

/* ------------------------ Types definitions ------------------------------- */
/*!
 * Signature placed after each allocated block and used to detect both memory
 * corruptions caused by overflowing allocated data as well as to validate
 * allocation pointers (in general) at free() time.
 */
typedef LwU32 OS_ALLOC_SIGNATURE;

/*!
 * Defines node of the singly-linked list tracking unallocated portions of heap.
 */
typedef struct OS_ALLOC_HDR_FREE OS_ALLOC_HDR_FREE;
struct OS_ALLOC_HDR_FREE
{
    OS_ALLOC_HDR_FREE  *pNext;
    LwU32               size;
};

/*!
 * Defines header of an allocated heap block.
 */
typedef struct
{
/*!
 * Total size of an allocated block including all overheads.
 */
#define LW_OS_ALLOC_HDR_USED_DATA_SIZE   23:0
/*!
 * Size of the memory waste introduced before allocated block due to (optional)
 * alignment. Used in free() to locate actual start of block.
 */
#define LW_OS_ALLOC_HDR_USED_DATA_WASTE 31:24
    LwU32   data;
} OS_ALLOC_HDR_USED;

/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/*!
 * Pointers to the special (heading / trailing respectively) elements of the
 * singly-linked list introduced to elliminate corner cases in list handling
 * and assuring that list is never empty and that elements at the begining /
 * end of the list are never acessed (inserted / removed).
 */
static OS_ALLOC_HDR_FREE  *OsAllocPHead;
static OS_ALLOC_HDR_FREE  *OsAllocPTail;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Minimal size of the free block (for minimal allocation request).
 * Used to determine if wasted memory should be tracked as free blocks.
 * Current define will allow tracking of any wasted memory as long as it
 * can allow an allocation of a single byte.
 */
#define OS_ALLOC_BLOCK_FREE_SIZE_MIN        \
    LW_MAX((sizeof(OS_ALLOC_HDR_USED) +     \
            lwrtosBYTE_ALIGNMENT +          \
            sizeof(OS_ALLOC_SIGNATURE)),    \
           sizeof(OS_ALLOC_HDR_FREE))

/*!
 * Defines for some typical alignments.
 * To be used within calling code instead of numeric constants.
 */
#define OS_ALLOC_ALIGNMENT_MIN          lwrtosBYTE_ALIGNMENT
#define OS_ALLOC_ALIGNMENT_DWORD        lwrtosBYTE_ALIGNMENT
#define OS_ALLOC_ALIGNMENT_PARAGRPAH    16
#define OS_ALLOC_ALIGNMENT_BLOCK        DMEM_BLOCK_SIZE

/*!
 * Trailing signature used to validate allocation pointer within free() requests
 * as well as to assure that write acessess to an allocated block didn't overrun
 * allocated space.
 * Simplest implementation is to use signature of constant value.
 */
#define OS_ALLOC_SIGNATURE_VALUE        0x5A5A5A5A

/* ------------------------ Public variables -------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * Initializes heap by creating a list of free blocks (single list node) and
 * inserting it between special nodes (head / tail) that simplify handling of
 * the corner cases in list operations.
 */
void
osAllocInitialize
(
    void   *pHeapStart,
    LwU32   heapSize
)
{
    OS_ALLOC_HDR_FREE  *pHead;
    OS_ALLOC_HDR_FREE  *pFree;
    OS_ALLOC_HDR_FREE  *pTail;
    LwU32               heapStart   = (LwU32)pHeapStart;
    LwU32               hdrFreeSize = sizeof(OS_ALLOC_HDR_FREE);

    //
    // Assures that the heap is properly aligned and that it's size allows for
    // the allocation of at least a single byte of usefull memory.
    //
    OS_ASSERT(LW_IS_ALIGNED(heapStart, OS_ALLOC_ALIGNMENT_MIN));
    OS_ASSERT(LW_IS_ALIGNED(heapSize,  OS_ALLOC_ALIGNMENT_MIN));
    OS_ASSERT(heapSize >= (OS_ALLOC_BLOCK_FREE_SIZE_MIN + 2 * hdrFreeSize));

    // Place the initial list nodes to proper offsets.
    pHead = (OS_ALLOC_HDR_FREE *)(heapStart);
    pFree = (OS_ALLOC_HDR_FREE *)(heapStart + hdrFreeSize);
    pTail = (OS_ALLOC_HDR_FREE *)(heapStart + heapSize - hdrFreeSize);

    // Link the initial list nodes into the singly-linked list.
    pHead->pNext = pFree;
    pFree->pNext = pTail;
    pTail->pNext = NULL;

    // Set size of the initial list nodes.
    pHead->size = hdrFreeSize;
    pFree->size = heapSize - 2 * hdrFreeSize;
    pTail->size = hdrFreeSize;

    // Initialize the global heap pointers of the heading / tailing nodes.
    OsAllocPHead = pHead;
    OsAllocPTail = pTail;
}

/*!
 * Allocates the memory on the heap honoring the requested alignemnt.
 */
void *
osMallocAligned
(
    LwU32   allocSize,
    LwU32   alignment
)
{
    void *pAllocation = NULL;

    if (allocSize == 0)
    {
        goto osMallocAligned_exit;
    }

    if (!ONEBITSET(alignment))
    {
        goto osMallocAligned_exit;
    }

    // For now we do not support alignment of more than 256 bytes.
    if (alignment > DMEM_BLOCK_SIZE)
    {
        goto osMallocAligned_exit;
    }

    // Everything else is aligned so do this early and be done with it.
    allocSize = LW_ALIGN_UP(allocSize, OS_ALLOC_ALIGNMENT_MIN);

    //
    // Note:    When using this code for multiple heaps use per-heap mutex and
    //          avoid global task suspension. For cases where single thread of
    //          control can be assured to work with heap no sync. is required.
    //
    vTaskSuspendAll();
    {
        OS_ALLOC_HDR_FREE *pPrev = OsAllocPHead;
        OS_ALLOC_HDR_FREE *pNode = OsAllocPHead->pNext;

        while (pNode != OsAllocPTail)
        {
            LwU32   start = (LwU32)pNode;
            LwU32   size  = pNode->size;
            LwU32   free1;
            LwU32   free2;
            LwU32   alloc;
            LwU32   sign;
            LwU32   end;

            // Account for the allocation header.
            alloc = start + sizeof(OS_ALLOC_HDR_USED);

            free1 = alloc;

            // Account for the allocation alignment.
            alloc = LW_ALIGN_UP(alloc, alignment);

            // Compute vaste of the allocation.
            free1 = alloc - free1;

            // Account for the allocation size.
            sign = alloc + allocSize;

            // Account for the allocation signature.
            end = sign + sizeof(OS_ALLOC_SIGNATURE);

            // First fit...
            if ((end - start) <= size)
            {
                if (free1 >= OS_ALLOC_BLOCK_FREE_SIZE_MIN)
                {
                    //
                    // An allocation alignment has wasted more memory than its
                    // required for the minimal allocaton so track this newly
                    // introduced free block...
                    //
                    pNode->size = free1;
                    start += free1;
                    size  -= free1;
                    free1  = 0;
                    pPrev  = pNode;
                }
                else
                {
                    // ... just use the block from start and live with waste.
                    pPrev->pNext = pNode->pNext;
                }

                free2 = size - (end - start);

                if (free2 >= OS_ALLOC_BLOCK_FREE_SIZE_MIN)
                {
                    //
                    // Free space following the new allocation is suffcieint for
                    // the minimal allocation so track this newly introduced
                    // free block.
                    //
                    pNode = (OS_ALLOC_HDR_FREE *)end;
                    pNode->size  = free2;
                    pNode->pNext = pPrev->pNext;
                    pPrev->pNext = pNode;
                    size -= free2;
                }

                //
                // Populate the allocation header while including information on
                // the alignment waste. This information will be used by free()
                // to locate actual start of the block to be freed.
                //
                ((OS_ALLOC_HDR_USED *)alloc)[-1].data =
                    DRF_NUM(_OS_ALLOC, _HDR_USED, _DATA_SIZE,  size) |
                    DRF_NUM(_OS_ALLOC, _HDR_USED, _DATA_WASTE, free1);
                //
                // Append the signature.
                // Note that the signature is not always placed right after the
                // usefull data, rather at the very end of block used for the
                // allocation. Difference is equal to @ref free2 bytes.
                //
                ((OS_ALLOC_SIGNATURE *)(start + size))[-1] = OS_ALLOC_SIGNATURE_VALUE;

                pAllocation = (void *)alloc;

                break;
            }

            pPrev = pNode;
            pNode = pNode->pNext;
        }
    }
    xTaskResumeAll();

osMallocAligned_exit:
    return pAllocation;
}

/*!
 * Allocates the memory using no alignemnt (aside from min required).
 */
void *
osMalloc
(
    LwU32   allocSize
)
{
    return osMallocAligned(allocSize, OS_ALLOC_ALIGNMENT_MIN);
}

/*!
 * TODO: Sahil
 *
 * Sahil will be implementing secure portion of this code.
 */
void *
osMallocSelwre
(
    LwU32   allocSize,
    LwU8    lvl
)
{
    void *pAllocation = NULL;

    /*
    // Assure that no one waste us for non-secure allocations.
    // TODO: Replace constant with proper define for non-secure requests.
    OS_ASSERT(lvl != 0);

    allocSize = LW_ALIGN_UP(allocSize, DMEM_BLOCK_SIZE);

    // Allocate it ...
    pAllocation = osMallocAligned(allocSize, DMEM_BLOCK_SIZE);
    if (pAllocation != NULL)
    {
        // ... and make it secure.
        _osAllocBlockLvlSet(pAllocation, allocSize);
    }
    */

    OS_HALT(); // Until implemented

    return pAllocation;
}

/*!
 * Frees the allocated memory coalescing it with the adjecent blocks (if any).
 */
void
osFree
(
    void   *pAllocation
)
{
    LwU32               data;
    LwU32               size;
    LwU32               waste;
    LwU32               start;
    LwU32               end;
    OS_ALLOC_HDR_FREE  *pPrev;
    OS_ALLOC_HDR_FREE  *pNext;

    // TODO: Sahil was proposing to just silently exist in this case.
    // If we skip this calling with NULL pointer will result in DMEM miss.
    OS_ASSERT(pAllocation != NULL);

    // Extract the data from the allocation header.
    data  = ((OS_ALLOC_HDR_USED *)pAllocation)[-1].data;
    size  = DRF_VAL(_OS_ALLOC, _HDR_USED, _DATA_SIZE,  data);
    waste = DRF_VAL(_OS_ALLOC, _HDR_USED, _DATA_WASTE, data);

    // First validation - that waste size is not corrupted.
    OS_ASSERT(waste < OS_ALLOC_BLOCK_FREE_SIZE_MIN);

    // Identify the whole block used for this allocation.
    start = (LwU32)pAllocation - sizeof(OS_ALLOC_HDR_USED) - waste;
    end   = start + size;

    // Second validation - assure that the signature is there (and intact).
    OS_ASSERT(((OS_ALLOC_SIGNATURE *)end)[-1] == OS_ALLOC_SIGNATURE_VALUE);

    pPrev = OsAllocPHead;
    pNext = OsAllocPHead->pNext;

    while (pPrev != OsAllocPTail)
    {
        LwU32   prevEnd = (LwU32)pPrev + pPrev->size;

        if (start < prevEnd)
        {
            break;
        }

        if (end > (LwU32)pNext)
        {
            pPrev = pNext;
            pNext = pNext->pNext;
            continue;
        }

        // At this point block-to-be-freed is located between pPrev and pNext.

        if ((start == prevEnd) &&
            (pPrev != OsAllocPHead))
        {
            // Coalesce the freed memory with the previous free block.
            pPrev->size += size;
        }
        else
        {
            OS_ALLOC_HDR_FREE *pNode;

            // Create new free block and insert it into the doubly linked list.
            pNode        = (OS_ALLOC_HDR_FREE *)start;
            pNode->size  = size;
            pNode->pNext = pPrev->pNext;
            pPrev->pNext = pNode;
            pPrev        = pNode;
        }

        if ((((LwU32)pPrev + pPrev->size) == (LwU32)pNext) &&
            (pNext != OsAllocPTail))
        {
            // Coalesce the feed memory with the next free block.
            pPrev->size += pNext->size;
            pPrev->pNext = pNext->pNext;
        }

        return;
    }

    OS_HALT();
}

/*!
 * TODO: Sahil
 *
 * As dislwssed offline this will be some osUnmap() interface or similar.
 */
void
osFreeSelwre
(
    void   *pAllocation
)
{
    //
    // TODO-s:
    // #1 - Check allocation valid and obtain the size (redundant to free()).
    // #2 - Wipe-out the data
    // #3 - Lower the security
    // #4 - call free() or leave this to LS/NS code.
    //

    OS_HALT(); // Until implemented
}

/* ------------------------ Private Functions ------------------------------- */

