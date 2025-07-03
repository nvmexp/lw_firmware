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
#include "pin.h"

/**
 * @brief Pins a page 
 * 
 * @see Note on PageState::referenceCount for semantics of pin.
 * 
 * Contract: KernelRangeDoesntWrap(physicalAddress, size) &&
 *           KernelAddressPageAligned(physicalAddress) &&
 *           KernelAddressPageAligned(size)
 * 
 * @param physicalAddress 
 * @param size 
 */
LibosStatus KernelMemoryPoolAcquire(LwU64 physicalAddress, LwU64 size)
{
    LwU64 unwindStartAddress = physicalAddress;

    KernelAssert(KernelRangeDoesntWrap(physicalAddress, size) &&
                 KernelAddressPageAligned(physicalAddress) &&
                 KernelAddressPageAligned(size));

    while (size)
    {
        MemoryPool * node;
        PageState * state = KernelPageStateSplit(physicalAddress, size, &node);

        if (!state) 
            goto unwind;               // Did we just try to pin an MMIO mapping?

        LwU64 pageSize = 1ULL << state->pageLog2;
        KernelAssert(pageSize <= size);

        size            -= pageSize;   // Can't underflow since split guarantees pageSize < size
        physicalAddress += pageSize;   // Can't overflow since caller guaranteed the physicalAddress range
                                       // was valid.

        
        if (state->referenceCount == PAGE_STATE_REFERENCE_COUNT_MAX)
            goto unwind;

        state->referenceCount++;
    }
    return LibosOk;

unwind:
    KernelMemoryPoolRelease(unwindStartAddress, physicalAddress - unwindStartAddress);
    return LibosErrorFailed;
}


/**
 * @brief Releases a pin on the range.
 * 
 * @see  PageState::referenceCount for semantics.
 * 
 * @note Since this function releases the referenceCount
 *       it is also used by the kernel to release 
 *       the ownership from the parent. 
 * 
 * Contract: KernelRangeDoesntWrap(physicalAddress, size) &&
 *           KernelAddressPageAligned(physicalAddress) &&
 *           KernelAddressPageAligned(size)
 * 
 * @param physicalAddress 
 * @param size 
 */
void KernelMemoryPoolRelease(LwU64 physicalAddress, LwU64 size)
{
    KernelAssert(KernelRangeDoesntWrap(physicalAddress, size) &&
                 KernelAddressPageAligned(physicalAddress) &&
                 KernelAddressPageAligned(size));

    while (size)
    {
        MemoryPool * node;
        PageState * state = KernelPageStateSplit(physicalAddress, size, &node);

        // @todo: Eliminate this once we have a path to prevent Releases on
        //        pinned mappings
        if (!state)
            return;

        LwU64 pageSize = 1ULL << state->pageLog2;
        KernelAssert(pageSize <= size);
        
        if (!--state->referenceCount) {
            // Ensure all pages in this range are marked as zero reference count
            for (LwU64 probe = physicalAddress; probe < (physicalAddress + (1ULL << state->pageLog2)); probe += LIBOS_CONFIG_MPU_PAGESIZE)
                KernelAssert(KernelPageStateByAddress(probe, 0)->referenceCount == 0);

            //KernelPrintf("Buddy: Free page %llx\n", physicalAddress);

            KernelBuddyFree(node->buddyTree, physicalAddress, state->pageLog2);     
        }

        size            -= pageSize;   // Can't underflow since split guarantees pageSize < size
        physicalAddress += pageSize;   // Can't overflow since caller guaranteed the physicalAddress range
                                       // was valid.
    }
}

