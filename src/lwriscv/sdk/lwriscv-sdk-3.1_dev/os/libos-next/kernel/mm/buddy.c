#include <lwtypes.h>
#include "buddy.h"
#include "libos-config.h"
#include "libbit.h"
#include "diagnostics.h"

/**
 *  Buddy tree
 *   
 *   Each node in the tree has a bitmap of the 
 *   sizes of blocks that are free in the subtree.
 *   
 *   The bitmap is organized from the the largest
 *   page size at bit 0, to the smallest at bit 63.
 *   This allows efficient traversal left, right,
 *   and parent.
 * 
 *   Allocated nodes do not have children:
 *      tree[iterator.index] == 0                   
 * 
 *   Fully free nodes do have have children
 *      tree[iterator.node] == iterator.orderMask 
 * 
 *   Only partially free nodes have children.
 * 
 * 
 */

BuddyIterator KernelBuddyIterator(LwU32 index, LwU64 orderLevel, LwU64 orderMask) {
    BuddyIterator r = {index, orderLevel, orderMask};
    return r;
}

BuddyIterator KernelBuddyChild(BuddyIterator i, LwBool isRight)
{
    return KernelBuddyIterator(i.index * 2 + isRight, i.orderLevel + 1, i.orderMask * 2);
}

LwBool KernelBuddyIsLeftChild(BuddyIterator i)
{
    return (i.index & 1) == 0;
}

BuddyIterator KernelBuddyChildLeft(BuddyIterator i)
{
    return KernelBuddyChild(i, LW_FALSE);
}

BuddyIterator KernelBuddyChildRight(BuddyIterator i)
{
    return KernelBuddyChild(i, LW_TRUE);
}

BuddyIterator KernelBuddyPeer(BuddyIterator node)
{
    return KernelBuddyIterator(node.index ^ 1, node.orderLevel, node.orderMask);
}

BuddyIterator KernelBuddyParent(BuddyIterator node)
{
    return KernelBuddyIterator(node.index / 2, node.orderLevel-1, node.orderMask / 2);
}

BuddyIterator KernelBuddyPage(Buddy * buddy, LwU64 pageAddress, LwU64 pageLog2)
{
    LwU64 address = pageAddress - buddy->physicalBase;
    LwU64 order = buddy->maximumPageLog2 - pageLog2;
    LwU64 index = (1ULL << order) + (address >> pageLog2);
    return KernelBuddyIterator(index, order, 1ULL << order);
}

LwU64 KernelBuddyIteratorPageSizeLog2(Buddy * buddy, BuddyIterator allocation)
{
    return buddy->maximumPageLog2 - allocation.orderLevel;
}

LwU64 KernelBuddyIteratorAddress(Buddy * buddy, BuddyIterator allocation)
{
    return buddy->physicalBase + ((allocation.index - allocation.orderMask) << KernelBuddyIteratorPageSizeLog2(buddy, allocation));    
}

/**
 * @brief Find an allocation at or after the searchAddress.
 *        Prefers to find an allocation exactly at the searchAddress.
 * 
 * @note: An incorrect answer from this function will not result
 *        in buddy tree corruption.  The 'allocateAtAddress' function
 *        will simply fail.
 * 
 * @param buddy 
 * @param searchAddress Starting address for search
 * @param pageLog2 Log2 of desired block size
 * @param outIterator Iterator of free block
 * @param outAddress Address of free block
 * @return LwBool 
 */
LwBool KernelBuddyFindFreeByAddress(Buddy * buddy, LwU64 searchAddress, LwU64 pageLog2, BuddyIterator * outIterator, LwU64 * outAddress)
{
    // Ensure we can satisfy the requested allocation
    if (pageLog2 > buddy->maxAlignment || 
        pageLog2 < buddy->minimumPageLog2) 
        return LW_FALSE;        
    
    // Compute the bit in the mask that represents this page size
    LwU64 orderMask = 1ULL << (buddy->maximumPageLog2 - pageLog2);

    // Start the tree traversal at the root.
    *outIterator = buddyRoot;
    LwU64 pageSize = 1ULL << buddy->maximumPageLog2;
    LwU64 address  = buddy->physicalBase;

    /**
     * Is there a page at least as large as the requested allocation in the tree?
     */
    if (!(buddy->tree[outIterator->index] & LibosBitAndBitsBelow(orderMask))) {
        KernelTrace("!!   %llx %llx\n", buddy->tree[outIterator->index], LibosBitAndBitsBelow(orderMask));
        return LW_FALSE;
    }

    /**
     *  We know a free block of sufficient size exists.
     *  Allocation MUST succeed after this point, we just need to find it.
     */

    do
    {
        LwU64 mask = buddy->tree[outIterator->index];
        
        // Assert that the current node contains a sufficiently sized free page
        KernelAssert(mask & LibosBitAndBitsBelow(orderMask));

        // Loop ilwariant for address callwlation
        KernelAssert(address == KernelBuddyIteratorAddress(buddy, *outIterator));

        // Is this block entirely free?
        if (mask == outIterator->orderMask) 
        { 
            /**
             *  Is the requested allocation inside the superblock
             */
            if (searchAddress < address || (searchAddress+(1ULL << pageLog2)) > (address+pageSize)) 
                *outAddress = address;         // No, just return the superblock start
            else
                *outAddress = searchAddress;   // Yes, the requested address was free

            return LW_TRUE;
        }

        // Update the page size for the current level
        pageSize >>= 1;

        LwBool isRight = searchAddress >= (address + pageSize);
        
        *outIterator = KernelBuddyChild(*outIterator, isRight);

        // Follow the the other subtree if this one isn't free.
        if (!(buddy->tree[outIterator->index] & LibosBitAndBitsBelow(orderMask))) {
            *outIterator = KernelBuddyPeer(*outIterator);
            isRight = !isRight;
        }

        // update the allocation address for the current level
        if (isRight)
            address += pageSize;
    } while (LW_TRUE);
}

/**
 * @brief Computes the free mask set at the superblock when
 *        splitting a completely free superblock.
 * 
 *        Consider a completely free superblock of 16k 
 *        Consider an allocation of 4k within.
 * 
 *        We progressively split pages until we're left
 *        with the desired allocation
 * 
 *        Set = (16kb)
 *        Set = (8kb, 8kb)
 *        Set = (8kb, 8kb, 4kb, 4kb)
 * 
 *        @note: The orderMasks have the biggest page at bit 0, 
 *               and the smallest page at bit 63.  
 *               
 *    Only pages smaller than the superblock can appear in the 
 *    superblock mask after splitting:
 *          LibosBitsAbove(superblockOrderMask);   // smallest bits at the top
 *  
 *    Similarly, no pages smaller than ththe allocationSize can
 *    appear.
 *          LibosBitAndBitsBelow(allocationOrderMask)
 * 
 *    LibosBitsAbove(superblockOrderMask) = ~(superblockOrderMask - 1)
 *    LibosBitAndBitsBelow(allocationOrderMask) = 2*allocationOrderMask - 1
 * 
 *
 * @param superBlockPower2 
 * @param allocationPower2 
 * @return LwU64 
 */
static LwU64 KernelBuddyBitmapFreeMaskAfterSplit(LwU64 superblockOrderMask, LwU64 allocationOrderMask)
{
    return LibosBitsAbove(superblockOrderMask) & LibosBitAndBitsBelow(allocationOrderMask);
}

BuddyIterator KernelBuddySplitFreeNode(Buddy * buddy, BuddyIterator superblock, LwU64 address, LwU64 orderMask)
{
    // Contract: This node must be fully free
    KernelAssert(buddy->tree[superblock.index] == superblock.orderMask);

    /**
     *   The superblock starts fully free.  We traverse downward to the node
     *   we desired to allocate.
     * 
     *   As we traverse, we mark each peer as free.
     *   We also mark the appropriate freemask for each node on the way down.
     * 
     *   @note: The freemasks above the superblock will need patching
     *          
     */
    BuddyIterator self = superblock;
    while (self.orderMask != orderMask)  
    {      
        buddy->tree[self.index] = KernelBuddyBitmapFreeMaskAfterSplit(self.orderMask, orderMask);

        if (address < (KernelBuddyIteratorAddress(buddy, self) + (1ULL<<(KernelBuddyIteratorPageSizeLog2(buddy, self)-1))))
            self = KernelBuddyChildLeft(self);
        else
            self = KernelBuddyChildRight(self);
        buddy->tree[KernelBuddyPeer(self).index] = KernelBuddyPeer(self).orderMask;      // Peer on path is free
    }

    // We're now at the node we're allocating    
    BuddyIterator allocated = self;

    // Mark this node as allocated
    buddy->tree[self.index] = 0;
    
    /**
     *   We've split the superblock into numerous powers of 2
     *   We also need to remove the superblock from the freelist.
     * 
     *   Recompute the freelists from peers on the way up.
     */
    self = superblock;
    LwU64  aclwmulatedFreeMask = KernelBuddyBitmapFreeMaskAfterSplit(superblock.orderMask, orderMask);

    while (self.index != buddyRoot.index)
    {
        BuddyIterator peer = KernelBuddyPeer(self);
        BuddyIterator parent = KernelBuddyParent(self);

        // Add our peers freelist to our mask
        aclwmulatedFreeMask |= buddy->tree[peer.index];
        
        // Once we reach a node that is correct we can stop.
        if (aclwmulatedFreeMask == buddy->tree[parent.index])
            break;

        // Update the mask
        buddy->tree[parent.index] = aclwmulatedFreeMask;

        // Move up a level
        self = parent;
    } 

    return allocated;
}


void KernelBuddyInternalFree(Buddy * buddy, BuddyIterator self)
{
    // Coalesce with adjacent free nodes
    while (buddy->tree[KernelBuddyPeer(self).index] == self.orderMask)
        self = KernelBuddyParent(self);

    // Free the page
    LwU64 aclwmulatedFreeMask = self.orderMask;
    buddy->tree[self.index] = aclwmulatedFreeMask;

    // Update the masks up to the root
    while (self.index != buddyRoot.index)
    {
        BuddyIterator peer = KernelBuddyPeer(self);
        BuddyIterator parent = KernelBuddyParent(self);

        // Accumulate free nodes from the peer
        aclwmulatedFreeMask |= buddy->tree[peer.index];

        // We're done when no updates are required to the parent
        // @note This is the common case.
        if (aclwmulatedFreeMask == buddy->tree[parent.index])
            return;

        // Update the parent free mask
        buddy->tree[parent.index] = aclwmulatedFreeMask;

        // Move to parent node
        self = parent;
    }     
}

LwBool KernelBuddyAllocate(Buddy * buddy, LwU64 searchAddress, LwU64 * address, LwU64 pageLog2) 
{
    KernelTrace("KernelBuddyAllocate(%llx, %lld)\n", searchAddress, pageLog2);
      
    // At this point we know we have a page free of order 'mask'
    // Let's find a page matching the search requirements
    BuddyIterator superblock;
    if (!KernelBuddyFindFreeByAddress(buddy, searchAddress, pageLog2, &superblock, address))
        return LW_FALSE;
    KernelTrace("KernelBuddyAllocate: KernelBuddyFindFreeByAddress(%llx) -> %llx in  sb:%llx ps:%llx\n", searchAddress, *address, KernelBuddyIteratorAddress(buddy, superblock), KernelBuddyIteratorPageSizeLog2(buddy, superblock));

    // Split the page
    LwU64 order = buddy->maximumPageLog2 - pageLog2;
    LwU64 mask = 1ULL << order;
    KernelBuddySplitFreeNode(buddy, superblock, *address, mask);
    KernelTrace("[!] KernelBuddyAllocate -> %llx\n", *address);
    return LW_TRUE;
}

void KernelBuddyFree(Buddy * buddy, LwU64 address, LwU64 pageLog2)
{
    KernelTrace("[!] KernelBuddyFree %llx %lld\n", address, pageLog2);

    KernelBuddyInternalFree(buddy, KernelBuddyPage(buddy, address, pageLog2));
}

LwU64 KernelBuddyTreeSize(LwU64 physicalBase, LwU64 physicalSize)
{
    // The buddy tree must be fully populated.  This implies 
    //  1. We must round the size up to the next power of 2
    //  2. We must ensure these new pages are reserved
    LwU64 treeSize = (physicalSize / LIBOS_CONFIG_MPU_PAGESIZE ) * 2 * sizeof(LwU64);
    
    return treeSize;
}

void KernelBuddyConstruct(Buddy * buddy, LwU64 * treeArray, LwU64 physicalBase, LwU64 physicalSize, LwU64 pageSizeLog2)
{
    // The tree is contained within the page
    buddy->tree = treeArray;
    
    buddy->maxAlignment = LibosMinU64(
            LibosBitHighest(physicalSize),   // Largest power of 2 page that fits within size
            LibosBitLowest(physicalBase));   // Alignment of the physical base
    
    buddy->maximumPageLog2 = LibosLog2GivenPow2(physicalSize);
    buddy->minimumPageLog2 = pageSizeLog2;
    buddy->physicalBase = physicalBase;
    // Mark the root as free
    // @note: No other fields need to be initialized as the children state of
    //        free/allocated nodes are considered undefined.
    buddy->tree[buddyRoot.index] = buddyRoot.orderMask;
}