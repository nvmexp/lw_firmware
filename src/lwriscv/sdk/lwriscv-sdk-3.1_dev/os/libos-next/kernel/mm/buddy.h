#include <lwtypes.h>

//
//  High level API
//
struct Buddy
{
    LwU64   physicalBase;
    LwU64   maxAlignment;
    LwU64   maximumPageLog2;
    LwU64   minimumPageLog2;
    LwU64 * tree;
};

typedef struct Buddy Buddy;

void    KernelBuddyConstruct(Buddy * buddy, LwU64 * treeArray, LwU64 physicalBase, LwU64 physicalSize, LwU64 pageSizeLog2);
LwBool  KernelBuddyAllocate(Buddy * buddy, LwU64 searchAddress, LwU64 * address, LwU64 pageLog2);
void    KernelBuddyFree(Buddy * buddy, LwU64 address, LwU64 pageLog2);

//
//  Low level API (for unit testing)
//
typedef struct BuddyIterator
{
    LwU64 index;
    LwU64 orderLevel;
    LwU64 orderMask;    
} BuddyIterator;

BuddyIterator KernelBuddyIterator(LwU32 index, LwU64 orderLevel, LwU64 orderMask);

static const BuddyIterator buddyRoot = {1, 0, 1ULL};

LwU64         KernelBuddyTreeSize(LwU64 physicalBase, LwU64 physicalSize);
BuddyIterator KernelBuddyChild(BuddyIterator i, LwBool isRight);
LwBool        KernelBuddyIsLeftChild(BuddyIterator i);
BuddyIterator KernelBuddyChildLeft(BuddyIterator i);
BuddyIterator KernelBuddyChildRight(BuddyIterator i);
BuddyIterator KernelBuddyPeer(BuddyIterator node);
BuddyIterator KernelBuddyParent(BuddyIterator node);
void          KernelBuddyNextFree(Buddy * buddy, BuddyIterator * searchPosition, LwU64 orderMask);
void          KernelBuddyInternalFree(Buddy * buddy, BuddyIterator self);
LwU64         KernelBuddyIteratorAddress(Buddy * buddy, BuddyIterator allocation);
BuddyIterator KernelBuddyPage(Buddy * buddy, LwU64 pageAddress, LwU64 pageLog2);
LwBool        KernelBuddyFindFreeByAddress(Buddy * buddy, LwU64 searchAddress, LwU64 pageLog2, BuddyIterator * candidate, LwU64 * freeAddress);