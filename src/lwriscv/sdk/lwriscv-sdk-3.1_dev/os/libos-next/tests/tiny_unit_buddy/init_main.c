#define LIBOS_UNIT_TEST_BUDDY

#include "kernel.h"
#include "libos.h"
#include "task.h"
#include "diagnostics.h"
#include "libbit.h"
#include "lwriscv-2.0/sbi.h"
#include "libos_log.h"
#include "kernel/mm/buddy.h"

LwU32  erandom32()
{
    static LwU32 state[16] = { 94, 15, 96, 76, 86, 179, 18, 91, 204, 20,238,247,231,217,208,189 };
    static LwU32 index = 0;
    LwU32 a, b, c, d;

    a = state[index];
    c = state[(index+13)&15];
    b = a^c^(a<<16)^(c<<15);
    c = state[(index+9)&15];
    c ^= (c>>11);
    a = state[index] = b^c;
    d = a^((a<<5)&0xDA442D24UL);
    index = (index + 15)&15;
    a = state[index];
    state[index] = a^b^d^(a<<2)^(b<<18)^(c<<28);
    return state[index];
}

LwU64 erandom64() {
    return erandom32() | ((erandom32()*1ULL)<<32);
}

void KernelSchedulerDirtyPagetables(void) {}

Task TaskInit;
Task *      KernelSchedulerGetTask() { return &TaskInit; }

void write64(LwU64 value)
{
    KernelMmioWrite(LW_PRGNLCL_FALCON_DEBUGINFO, value >> 32);
    while (KernelMmioRead(LW_PRGNLCL_FALCON_DEBUGINFO))    
        ;
    KernelMmioWrite(LW_PRGNLCL_FALCON_DEBUGINFO, value & 0xFFFFFFFF);
    while (KernelMmioRead(LW_PRGNLCL_FALCON_DEBUGINFO))    
        ;  
}

// Directed test for KernelBuddyFindFreeByAddress
BuddyIterator nodeByIndex(LwU32 row, LwU32 col) {
    if (row == 0)
        return buddyRoot;
    return KernelBuddyChild(nodeByIndex(row-1, col), col & (1ULL << row));
}

static LwBool eq(BuddyIterator a, BuddyIterator b) {
    return a.index == b.index && a.orderLevel == b.orderLevel && a.orderMask == b.orderMask;
}

/*
    Validates iterator API ensures ilwariants
        buddyParent
        KernelBuddyChild
        KernelBuddyIsLeftChild
        KernelBuddyChildLeft
        KernelBuddyChildRight
        KernelBuddyPeer
        KernelBuddyParent
        KernelBuddyPage
*/

void basicTest()
{
    for (LwU32 col = 0; col < 16; col++)
    {
        BuddyIterator node = nodeByIndex(4, col);

        // Transitive parent/child relationship
        KernelPanilwnless(eq(KernelBuddyParent(KernelBuddyChildLeft(node)), node) &&
                    eq(KernelBuddyParent(KernelBuddyChildRight(node)), node));

        // child detection
        KernelPanilwnless(KernelBuddyIsLeftChild(KernelBuddyChildLeft(node)) &&
                    !KernelBuddyIsLeftChild(KernelBuddyChildRight(node)));

        // Peer detection
        KernelPanilwnless(eq(KernelBuddyPeer(KernelBuddyChildLeft(node)), KernelBuddyChildRight(node)));
        KernelPanilwnless(eq(KernelBuddyPeer(KernelBuddyChildRight(node)), KernelBuddyChildLeft(node)));

        // KernelBuddyPage / KernelBuddyIteratorAddress / KernelBuddyIteratorPageSizeLog2
        Buddy buddy = {
            0,      // physicalBase      
            0,      // maxAlignment
            16,      // maximumPageLog2 (2**(16-12) == 16 pages)
            12,      // minimumPageLog2
            0       // tree
        };
        
        // Address translation ilwariant
        KernelPanilwnless(KernelBuddyIteratorAddress(&buddy, KernelBuddyPage(&buddy, 4096*col, 0)) == 4096*col);
    }
}

void buddyIlwariant(Buddy * buddy, BuddyIterator node)
{
    // Is this a leaf node?
    if (node.orderLevel == buddy->minimumPageLog2)
        return;

    // Full allocated nodes also have no children
    if (!buddy->tree[node.index])
        return;

    // Fully free nodes also have no children
    if (buddy->tree[node.index] == node.orderMask) 
        return;

    LwU64 childMask = 
        buddy->tree[KernelBuddyChildLeft(node).index] | 
        buddy->tree[KernelBuddyChildRight(node).index];

    // Children free list MUST exist in parent
    KernelPanilwnless(buddy->tree[node.index] == childMask);

    buddyIlwariant(buddy, KernelBuddyChildLeft(node));
    buddyIlwariant(buddy, KernelBuddyChildRight(node));   
}

void directedAllocate()
{
    LwU64 tree[32];
    Buddy buddy;
    memset(&tree[0], 0, sizeof(tree));
    KernelBuddyConstruct(&buddy, &tree[0], 0, 65536, 12 /* 4kb page size */);

    // Mark the entire address space as free
    tree[buddyRoot.index] = buddyRoot.orderMask;
    buddyIlwariant(&buddy, buddyRoot);

    // Allocate
    for (LwU64 addr = 0; addr != 16*4096; addr+=4096) {
        LwU64 allocatedAddress = 0;
        KernelPanilwnless(KernelBuddyAllocate(&buddy, addr, &allocatedAddress, 12));
        KernelPanilwnless(allocatedAddress == addr);
        buddyIlwariant(&buddy, buddyRoot);
    }

    // Ensure all memory is allocated
    KernelPanilwnless(tree[buddyRoot.index] == 0);

    // Free
    for (LwU64 addr = 0; addr != 16*4096; addr+=4096) {
        KernelBuddyInternalFree(&buddy, KernelBuddyPage(&buddy, addr, 12));
        buddyIlwariant(&buddy, buddyRoot);
    }

    // Ensure all memory is free
    KernelPanilwnless(tree[buddyRoot.index] == 1);
}

// The test runs directly in supervisor mode
__attribute__((used)) LIBOS_NORETURN  void KernelInit()  
{
    KernelMpuLoad();

    LibosPrintf("Hello world from bare metal.\n");

    basicTest();

    directedAllocate();

    SeparationKernelShutdown();
}

void putc(char ch) {
    KernelMmioWrite(LW_PRGNLCL_FALCON_DEBUGINFO, ch);
    while (KernelMmioRead(LW_PRGNLCL_FALCON_DEBUGINFO))    
        ;
}


