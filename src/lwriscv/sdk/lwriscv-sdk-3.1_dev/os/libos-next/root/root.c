#include "root.h"
#include "kernel/lwriscv-2.0/sbi.h"
#include "peregrine-headers.h"
#include "libprintf.h"
#include "libos.h"
#include "libbit.h"
#include "libbootfs.h"


typedef void (*PKernelInit)(
    LwU64 initArg0, 
    LwU64 initArg1, 
    LwU64 initArg2, 
    LwU64 initArg3, 
    LwU64 memoryStart, 
    LwU64 memorySize,
    LwU64 kernelDataStart,
    LwU64 imageStart);

//
//  Memory onlining/offlining
//

void Panic();

#   define Panilwnless(cond)                  \
        if (!(cond))                                \
        {                                           \
            LibosPrintf("Runtime failure: " #cond " @ " __FILE__ ":%d\n", __LINE__);  \
            Panic();                         \
        }


#define MAX_NODE_SIZE       (1ULL << 32) // 4gb

#define MAX_PAGE_LOG2       20
#define MAX_PAGE_SIZE       (2ULL<<MAX_PAGE_LOG2)   // 2mb
#define MAX_PAGES_PER_NODE  (MAX_NODE_SIZE / MAX_PAGE_SIZE)

struct Memory
{
    LwU64  partitionAllowedAllocatorsMask;

    // 4GB / 2MB = 2048 bits = 256 bytes
    LwU64  bitmap[MAX_NODE_SIZE / (64 * MAX_PAGE_SIZE)];

    LwU8   referenceCount;   // How many partitions have this memory mapped?
    LwU64  begin, end;       // [begin, end)
};


LwBool awaitingPartitionSpawnCompletion = LW_FALSE;
LwU64  partitionSpawnCallingPartition;
LwBool awaitingPartitionExitHandleCompletion = LW_FALSE;
LwU64  partitionExitCallingPartition;
LwU64  partitionExitNotificationTargetPartition;
// 128 nodes x 256bytes == 32kb
struct Memory memoryNodes[LIBOS_MEMORY_NODE_COUNT] = {0};


struct LocalPmp
{
    LwU64 base;
    LwU64 size;
    LwU32 accessMode;
} rootPartitionPmp[16] = {0};

struct Partition
{
    LwU16 freePmpMask;  // Mask of free PMP entries
    struct Pmp
    {
        LwU64 base;
        LwU64 size;
        LwU32 accessMode;
    } pmp[16];
    struct {
        LwU32 code[6];
        LwU64 args[4];
    } thunk;
} partitions[LIBOS_MAX_PARTITION_COUNT];
 
__attribute__((aligned((GLOBAL_PAGE_SIZE)))) union {
    LwU8   raw[GLOBAL_PAGE_SIZE];
    struct GlobalPage page;
} globalPage;
 
LwU64 globalPortFreeMask[LWOS_ROOTP_GLOBAL_PORT_MAX / 64];
LwU8 globalPortOwner[LWOS_ROOTP_GLOBAL_PORT_MAX];

static LwBool probeNaturalBlockByAddress(LwU64 node, LwU64 base, LwU64 pageSize, LwBool doAllocation)
{
    LwU64 pageSizeLog2 = LibosLog2GivenPow2(pageSize);

    pageSizeLog2 -= MAX_PAGE_LOG2;   
    LwU64 pageIndex = (base - memoryNodes[node].begin) / MAX_PAGE_SIZE;

    if (pageSizeLog2 < 6) {
        // can check a single word
        LwU64 mask = memoryNodes[node].bitmap[pageIndex / 64];
        LwU64 checkMask = ((1ULL << pageSizeLog2) - 1) << (pageIndex % 64);
        if ((mask & checkMask) != checkMask)
            return LW_FALSE;

        if (!doAllocation)
            return LW_TRUE;
        memoryNodes[node].bitmap[pageIndex / 64] &= ~checkMask;        
        return LW_TRUE;
    } else {
        // Probe
        LwU64 firstIndex = pageIndex / 64;
        LwU64 endIndex = firstIndex + (1ULL << (pageSizeLog2 - 6));
        for (LwU64 i = pageIndex / 64; i != endIndex; i++)
            if (memoryNodes[node].bitmap[i] != -1ULL) {
                return LW_FALSE;
            }
        
        if (!doAllocation) {
            return LW_TRUE;
        }

        // Allocate
        for (LwU64 i = pageIndex / 64; i != endIndex; i++)
            memoryNodes[node].bitmap[i] = 0;
                
        return LW_TRUE;
    }
}

static LwBool probeBlock(LwU64 node, LwU64 * base, LwU64 alignment, LwU64 size)
{
    LwU64 i = memoryNodes[node].begin;
    LwU64 candidate;
    LwU64 end = memoryNodes[node].end;

    // Align the start address
    i = (i + alignment - 1) &~ (alignment -1);
    candidate = i;

    while (i < end) 
    {
        // Probe a single block and move the candidate forward if not free
        if (!probeNaturalBlockByAddress(node, i, alignment, LW_FALSE /* don't allocate yet */)) {
            i += alignment; // advance
            candidate = i;  // set as next candidate
            continue;
        }

        // Move to next block
        i += alignment; 

        if ((candidate + size) <= i) {
            *base = candidate;
            return LW_TRUE;
        }
    }
    return LW_FALSE;
}


static void allocateBlock(LwU64 node, LwU64 base, LwU64 alignment, LwU64 size)
{
    // Sweep through an mark the region as allocated
    LwU64 i = base;
    for (; i < (base + size); i+=alignment) 
        (void)probeNaturalBlockByAddress(node, i, alignment, LW_TRUE /* allocate */);
}


static LwBool validateInterval(LwU64 begin, LwU64 end)
{
    return begin <= end;
}

static LwBool intervalContains(LwU64 parentBegin, LwU64 parentEnd, LwU64 begin, LwU64 end)
{
    return validateInterval(begin, end) && begin >= parentBegin && end <= parentEnd;
}


static LwBool localPmpIndex(LwU64 base, LwU64 size, LwU64 * index, LwU32 accessMode)
{
    for (LwU64 i = 0; i < 16; i++) 
        if (intervalContains(rootPartitionPmp[i].base, rootPartitionPmp[i].base + rootPartitionPmp[i].size,
                             base, base + size) && 
            (rootPartitionPmp[i].accessMode & accessMode) == accessMode)
        {
            *index = i;
            return LW_TRUE;
        }
    return LW_FALSE;
}

static LwBool remoteHasMapping(LwU64 callingPartition, LwU64 base, LwU64 size, LwU32 accessMode)
{
    if (callingPartition == 0)
        return LW_TRUE;

    for (LwU64 i = 0; i < 16; i++) 
        if (intervalContains(partitions[callingPartition].pmp[i].base, partitions[callingPartition].pmp[i].base + partitions[callingPartition].pmp[i].size,
                             base, base + size) &&
            (partitions[callingPartition].pmp[i].accessMode & accessMode) == accessMode)
            return LW_TRUE;
    return LW_FALSE;
}


LibosStatus LwosRootMemoryOffline(LwU8 node)
{
    Panilwnless(node < LIBOS_MEMORY_NODE_COUNT);

    // Is this memory node already online somewhere?
    if (memoryNodes[node].referenceCount)
        return LibosErrorAccess;

    memoryNodes[node].begin = 0;
    memoryNodes[node].end   = 0;        

    return LibosOk;
}

static LibosStatus lwosRootMemoryOnline(LwU8 node, LwU64 begin, LwU64 size, LwU64 partitionAllowedAllocatorsMask)
{
    Panilwnless(node < LIBOS_MEMORY_NODE_COUNT);

    // Is this memory node already online somewhere?
    if (memoryNodes[node].referenceCount)
        return LibosErrorAccess;

    // Check bitmap size
    if (size > MAX_NODE_SIZE)
        return LibosErrorOutOfMemory;

    // Ensure the region is within FBGPA
    // @todo: ITCM and DTCM should also be allowed
    if (!intervalContains(LW_RISCV_AMAP_FBGPA_START, LW_RISCV_AMAP_FBGPA_END, begin, begin+size))
        return LibosErrorArgument;

    // Initialize the node
    memoryNodes[node].begin = begin;
    memoryNodes[node].end   = begin+size;
    memoryNodes[node].partitionAllowedAllocatorsMask = partitionAllowedAllocatorsMask;

    // Mark memory as free
    for (LwU64 i = 0; i < size / MAX_PAGE_SIZE; i++)
        memoryNodes[node].bitmap[i/64] |= 1ULL << (i % 64);

    return LibosOk;
}

static LibosStatus lwosRootMemoryAllocateFromNode(LwU32 partitionOwner, LwU64 node, LwU64 * outAddress, LwU64 size, LwU64 alignment, LwU32 accessMode)
{
    // Is the block size a power of 2?
    if (LibosBitLowest(alignment) != alignment)
        return LibosErrorArgument;

    // Round size up by alignment
    size = (size + alignment - 1) &~ (alignment - 1);

    // Find a free PMP entry and map it
    LwU64 pmpMask = LibosBitLowest(partitions[partitionOwner].freePmpMask);
    if (!pmpMask)
        return LibosErrorOutOfMemory;

    // Find a candidate block
    LwU64 base;
    if (!probeBlock(node, &base, alignment, size))
        return LibosErrorOutOfMemory;

    // Which PMP entry locally contains this memory?
    LwU64 localIndex;
    if (!localPmpIndex(base, size, &localIndex, accessMode))
        return LibosErrorAccess;

    // Allocate the block
    allocateBlock(node, base, alignment, size);

    // Allocate the PMP entry
    LwU64 remoteIndex = LibosLog2GivenPow2(pmpMask);
    partitions[partitionOwner].freePmpMask &= ~pmpMask;

    // Update the shadow PMP entry
    partitions[partitionOwner].pmp[remoteIndex].base = base;
    partitions[partitionOwner].pmp[remoteIndex].size = size;
    partitions[partitionOwner].pmp[remoteIndex].accessMode = accessMode;

    // Map the PMP entry        
    SeparationKernelPmpMap(
        localIndex,
        partitionOwner,
        remoteIndex,
        accessMode,
        base,
        size);

    // Return the address
    *outAddress = base;

    return LibosOk;
}

static LibosStatus lwosRootPartitionMapPhysical(LwU64 callingPartition, LwU32 targetPartition, LwU64 base, LwU64 size, LwU32 accessMode)
{
    // Does the calling partition have access to this memory?
    if (!remoteHasMapping(callingPartition, base, size, accessMode)) 
        return LibosErrorAccess;

    // Which PMP entry locally contains this memory?
    LwU64 localIndex;
    if (!localPmpIndex(base, size, &localIndex, accessMode)) 
        return LibosErrorAccess;

    // Find a free PMP entry and map it
    LwU64 pmpMask = LibosBitLowest(partitions[targetPartition].freePmpMask);
    if (!pmpMask) {
        return LibosErrorOutOfMemory;        
    }
    // Allocate the PMP entry
    LwU64 remoteIndex = LibosLog2GivenPow2(pmpMask);
    partitions[targetPartition].freePmpMask &= ~pmpMask;

    // Update the shadow PMP entry
    partitions[targetPartition].pmp[remoteIndex].base = base;
    partitions[targetPartition].pmp[remoteIndex].size = size;
    partitions[targetPartition].pmp[remoteIndex].accessMode = accessMode;

    // Map the PMP entry        
    SeparationKernelPmpMap(
        localIndex,
        targetPartition,
        remoteIndex,
        accessMode,
        base,
        size);

    return LibosOk;    
}

static LibosStatus lwosRootMemoryAllocate(LwU32 partitionOwner, LwU64 * outAddress, LwU64 size, LwU64 alignment, LwU32 accessMode)
{
    Panilwnless(partitionOwner < LIBOS_MAX_PARTITION_COUNT);    
    LwU64 partitionMask = 1ULL << partitionOwner;

    for (LwU64 i = 0; i < LIBOS_MEMORY_NODE_COUNT; i++) {
        // Only consider nodes we're allowed to assign memory to
        if (!(memoryNodes[i].partitionAllowedAllocatorsMask & partitionMask))
            continue;

        LibosStatus status = lwosRootMemoryAllocateFromNode(partitionOwner, i, outAddress, size, alignment, accessMode);
        if (status == LibosOk)
            return status;        
    }

    return LibosErrorOutOfMemory;
}

static LwBool lwosRootMemoryFree(LwU32 partitionOwner, LwU64 base, LwU64 size)
{
    for (LwU32 memIdx = 0; memIdx < LIBOS_MEMORY_NODE_COUNT; memIdx++)
    {
        struct Memory *node = &memoryNodes[memIdx];
        if ((node->partitionAllowedAllocatorsMask & (1ULL << partitionOwner)) == 0)
        {
            continue;
        }
        if (!intervalContains(node->begin, node->end, base, base + size))
        {
            continue;
        }

        LibosPrintf("LWOS-Root: Releasing memory for partition %d: %016llx-%016llx\n",
            partitionOwner, base, base + size);

        LwU64 firstPageIdx = (base - node->begin) / MAX_PAGE_SIZE;
        LwU64 lastPageIdx = (base + size - node->begin) / MAX_PAGE_SIZE - 1;
        LwU64 firstBitmapIdx = firstPageIdx / 64;
        LwU64 lastBitmapIdx = lastPageIdx / 64;
        firstPageIdx %= 64;
        lastPageIdx %= 64;

        node->bitmap[firstBitmapIdx] |= ~((1ULL << firstPageIdx) - 1);
        for (LwU64 i = firstBitmapIdx + 1; i <= lastBitmapIdx - 1; i++)
        {
            node->bitmap[i] = 0xFFFFFFFFFFFFFFFFULL;
        }
        node->bitmap[lastBitmapIdx] |= (1ULL << (lastPageIdx + 1)) - 1;

        return LW_TRUE;
    }
    return LW_FALSE;
}

__attribute__((used,naked)) void Thunk()
{
    __asm("auipc t0, 0;"
          "ld    sp, 24(t0);"
          "ld    a7, 32(t0);"
          "ld    t0, 40(t0);"
          "jr    t0;"
          "nop;"
            :::);    
}


// @note: This code runs in the target partition!
// Entry point for remote partition
// Sets up .data section
static void lwosRootPartitionInitStub(LwU64 numaBegin, LwU64 memorySize, LibosBootFsArchive * image, LwU64 partition, LwU64 globalPort, LwU64 callingPartition)
{
    /* 
        Crack open the kernel image
    */
    LibosBootfsRecord * kernelImage = LibosBootFileFirst(image);
    LibosBootfsExelwtableMapping * text = LibosBootFindMappingText(image, kernelImage);
    LibosBootfsExelwtableMapping * data = LibosBootFindMappingData(image, kernelImage);

    /*
        Copy the kernel data section into the start of the heap 
    */
    LwU64 dataStart = numaBegin;
    memcpy((void *)numaBegin, (void *)((LwU64)image + data->offset), data->length);
    numaBegin += data->length;
    memorySize -= data->length;

    /* 
        Find the entry point
    */
    LibosElf64Header * elf = LibosBootFindElfHeader(image, kernelImage);
    LwU64 a = elf->entry - text->virtualAddress + text->offset + (LwU64)image;

    // Pass partitionId, imageStart, and libosEntrypoint to the global page
    globalPage.page.args.libosInit.partitionId = partition;
    globalPage.page.args.libosInit.imageStart = (LwU64) image;
    globalPage.page.args.libosInit.libosEntrypoint = a;
    globalPage.page.args.libosInit.parentGlobalPort = globalPort;

    /*
        Call the normal entry point to initialize the kernel (nothing more)
    */
    ((PKernelInit)a)(LWOS_A0(LWOS_VERB_LIBOS_INIT, 0), numaBegin, memorySize, (LwU64) &globalPage, sizeof(globalPage), LWOS_ROOT_PARTITION, 0, dataStart);  
}

LwU64 partitionInit_Partition;
static LibosStatus lwosRootPartitionInit(LwU64 callingPartition, LwU64 targetPartition, LwU64 image, LwU64 numaBegin, LwU64 memorySize, LwU32 globalPort)
{
    LibosStatus status;

    // @bug: Todo are we this partitions parent?

    // Copy in the reference thunk
    memcpy(&partitions[targetPartition].thunk.code[0], (void *)(LwU64)Thunk, sizeof(partitions[targetPartition].thunk.code));

    // Set up the context arguments
    partitions[targetPartition].thunk.args[0] = numaBegin + 4096 /* stackPointer */;
    partitions[targetPartition].thunk.args[1] = numaBegin + 4096 /* dataSection */;
    partitions[targetPartition].thunk.args[2] = (LwU64)lwosRootPartitionInitStub;

    // Create a PMP Entry for the partition
    // @bug: This 

    // Map the global page to the partition
    status = lwosRootPartitionMapPhysical(callingPartition, targetPartition, (LwU64) &globalPage, sizeof(globalPage), PMP_ACCESS_MODE_READ | PMP_ACCESS_MODE_WRITE | PMP_ACCESS_MODE_EXELWTE);
    if (status != LibosOk)
    {
        return status;
    }

    // Set the entry point
    SeparationKernelEntrypointSet(targetPartition, (LwU64)&partitions[targetPartition].thunk);

    // Call the newly minted partition to complete initilization
    awaitingPartitionSpawnCompletion = LW_TRUE;
    partitionSpawnCallingPartition = callingPartition;
    partitionInit_Partition = targetPartition;
    SeparationKernelPartitionSwitch(numaBegin + 4096, memorySize - 4096, image, targetPartition, globalPort, targetPartition);
}

static __attribute__((noreturn)) LibosStatus lwosRootPartitionExitContinuation()
{
    for (LwU64 p = partitionExitNotificationTargetPartition; p < LIBOS_MAX_PARTITION_COUNT; p++)
    {
        if ((globalPage.page.initializedPartitionMask & (1ULL << p)) == 0)
        {
            continue;
        }

        for (LwU32 seq = 0; seq < LWOS_ROOTP_GLOBAL_PORT_MAX; seq++)
        {
            if (
                (globalPortFreeMask[seq / 64] & (1ULL << (seq % 64))) == 0 &&
                globalPortOwner[seq] == partitionExitCallingPartition && (
                    (globalPage.page.globalPortGrants[seq].partitionsGrantSend & (1ULL << p)) != 0 ||
                    (globalPage.page.globalPortGrants[seq].partitionsGrantRecv & (1ULL << p)) != 0
                )
            )
            {
                partitionExitNotificationTargetPartition = p;
                awaitingPartitionExitHandleCompletion = LW_TRUE;
                SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_LIBOS_PARTITION_EXIT_NOTIFICATION, partitionExitCallingPartition), 0, 0, 0, 0, p);
            }
        }
    }

    for (LwU32 seq = 0; seq < LWOS_ROOTP_GLOBAL_PORT_MAX; seq++)
    {
        if (
            (globalPortFreeMask[seq / 64] & (1ULL << (seq % 64))) == 0 &&
            globalPortOwner[seq] == partitionExitCallingPartition
        )
        {
            globalPortFreeMask[seq / 64] |= 1ULL << (seq % 64);
            globalPortOwner[seq] = 0;
            globalPage.page.globalPortStatuses[seq].partitionsPendingSend = 0;
            globalPage.page.globalPortStatuses[seq].partitionsPendingRecv = 0;
            globalPage.page.globalPortGrants[seq].partitionsGrantSend = 0;
            globalPage.page.globalPortGrants[seq].partitionsGrantRecv = 0;
        }
    }

    for (LwU64 p = 0; p < LIBOS_MAX_PARTITION_COUNT; p++)
    {
        if (globalPage.page.initializedPartitionMask & (1ULL << p))
        {
            SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_LIBOS_RUN, 0), 0, 0, 0, 0, p);
        }
    }

    LibosPrintf("LWOS-Root: No partition to schedule, shutting down...\n");
    SeparationKernelShutdown();
}

static __attribute__((noreturn)) LibosStatus lwosRootPartitionExit(LwU64 callingPartition)
{
    globalPage.page.initializedPartitionMask &= ~(1ULL << callingPartition);
    partitionExitCallingPartition = callingPartition;
    partitionExitNotificationTargetPartition = 0;

    struct Partition *info = &partitions[callingPartition];
    for (LwU32 pmpIdx = 0; pmpIdx < 16; pmpIdx++)
    {
        if ((info->freePmpMask & (1 << pmpIdx)) != 0)
        {
            continue;
        }

        LwU64 base = info->pmp[pmpIdx].base;
        LwU64 size = info->pmp[pmpIdx].size;

        lwosRootMemoryFree(callingPartition, base, size);
    }
    info->freePmpMask = 0xFFFF;

    lwosRootPartitionExitContinuation();
}

static LibosStatus lwosRootAdvisePmp(LwU64 index, LwU64 base, LwU64 size, LwU32 pmpAccessMode)
{
    if (index >= 16)
        return LibosErrorArgument;

    rootPartitionPmp[index].base = base;
    rootPartitionPmp[index].size = size;
    rootPartitionPmp[index].accessMode = pmpAccessMode;

    return LibosOk;
}

static LibosStatus lwosRootGlobalPortAllocate(LwU64 callingPartition, LwU32 *port)
{
    for (LwU32 i = 0; i < sizeof(globalPortFreeMask) / sizeof(*globalPortFreeMask); i++)
    {
        LwU64 lowestBit = LibosBitLowest(globalPortFreeMask[i]);
        if (lowestBit == 0ULL)
        {
            continue;
        }

        globalPortFreeMask[i] &= ~lowestBit;
        LwU32 seq = i * 64 + LibosLog2GivenPow2(lowestBit);
        globalPortOwner[seq] = callingPartition;
        globalPage.page.globalPortGrants[seq].partitionsGrantSend =
            globalPage.page.globalPortGrants[seq].partitionsGrantRecv = 1ULL << callingPartition;
        *port = LWOS_ROOTP_GLOBAL_PORT_BUILD(callingPartition, seq);
        return LibosOk;
    }
    return LibosErrorOutOfMemory;
}

static LibosStatus lwosRootGlobalPortGrant(LwU64 callingPartition, LwU32 port, LwU64 targetPartition, LwosGlobalPortGrant mask)
{
    port = LWOS_ROOTP_GLOBAL_PORT_SEQ(port);

    if (port >= LWOS_ROOTP_GLOBAL_PORT_MAX || (globalPortFreeMask[port / 64] & (1ULL << (port % 64))) != 0)
    {
        return LibosErrorArgument;
    }

    if ((mask & LWOS_ROOTP_GLOBAL_PORT_GRANT_SEND) != 0 &&
        (globalPage.page.globalPortGrants[port].partitionsGrantSend & (1ULL << callingPartition)) == 0)
    {
        return LibosErrorAccess;
    }

    if ((mask & LWOS_ROOTP_GLOBAL_PORT_GRANT_RECV) != 0 &&
        (globalPage.page.globalPortGrants[port].partitionsGrantRecv & (1ULL << callingPartition)) == 0)
    {
        return LibosErrorAccess;
    }

    if ((mask & LWOS_ROOTP_GLOBAL_PORT_GRANT_SEND) != 0)
    {
        globalPage.page.globalPortGrants[port].partitionsGrantSend |= 1ULL << targetPartition;
    }

    if ((mask & LWOS_ROOTP_GLOBAL_PORT_GRANT_RECV) != 0)
    {
        globalPage.page.globalPortGrants[port].partitionsGrantRecv |= 1ULL << targetPartition;
    }

    return LibosOk;
}

void RootPartitionEntry(LwU64 a0, LwU64 a1, LwU64 a2, LwU64 a3, LwU64 a4, LwU64 a5, LwU64 a6, LwU64 a7)
{
    // @bug: Is this the register SK uses for calling partition?
    LwU64 callingPartition = a5;
    
    // Verb is upper bits of a0
    LwU32 verb = LWOS_A0_VERB(a0);
    LwU64 a0_56 = LWOS_A0_ARG(a0);

    if (awaitingPartitionSpawnCompletion) {
        awaitingPartitionSpawnCompletion = LW_FALSE;

        if (verb != LWOS_VERB_ROOTP_INITIALIZE_COMPLETED || callingPartition != partitionInit_Partition)
            SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN, LibosErrorAccess), 0, 0, 0, 0, callingPartition);
        
        // Set the entry point
        partitions[callingPartition].thunk.args[2] = a1;

        SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN, LibosOk), 0, 0, 0, 0, partitionSpawnCallingPartition);
    }

    if (awaitingPartitionExitHandleCompletion)
    {
        awaitingPartitionExitHandleCompletion = LW_FALSE;

        if (verb != LWOS_VERB_ROOTP_PARTITION_EXIT_HANDLE_COMPLETE || callingPartition != partitionExitNotificationTargetPartition)
        {
            SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN, LibosErrorAccess), 0, 0, 0, 0, callingPartition);
        }

        partitionExitNotificationTargetPartition++;
        lwosRootPartitionExitContinuation();
    }

    // Were we asked to initialize?
    switch(verb)
    {
        case LWOS_VERB_ROOTP_INITIALIZE:
        {
            LibosPrintf("LWOS-Root: LwosRootInitialize()\n");
            for (LwU32 i = 0; i < LIBOS_MAX_PARTITION_COUNT; i++)
                partitions[i].freePmpMask = 0xFFFF;
            
            for (LwU32 i = 0; i < sizeof(globalPortFreeMask) / sizeof(*globalPortFreeMask); i++)
                globalPortFreeMask[i] = 0xFFFFFFFFFFFFFFFFULL;

            SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN, LibosOk), 0, 0, 0, 0, callingPartition);
            break;
        }

        case LWOS_VERB_ROOTP_INITIALIZE_ADVISE_PMP:
        {
            LibosPrintf("LWOS-Root: LwosRootAdvisePmp(index:%lld base:%016llx size:%016llx pmpAccess:%llx)", a0_56, a1, a2, a3);
            LibosStatus status = lwosRootAdvisePmp(a0_56, a1, a2, a3);
            LibosPrintf("LWOS-Root: -> status:%d\n", status);
            SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN, status), 0, 0, 0, 0, callingPartition);
            break;
        }

        case LWOS_VERB_ROOTP_MEMORY_ONLINE:
        {
            LibosPrintf("LWOS-Root: LwosRootMemoryOnline(node:%lld base:%016llx size:%016llx partitionMask:%llx)", a0_56, a1, a2, a3);
            LibosStatus status = lwosRootMemoryOnline(a0_56, a1, a2, a3);
            LibosPrintf("LWOS-Root: -> status:%d\n", status);
            SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN, status), 0, 0, 0, 0, callingPartition);
            break;
        }

        case LWOS_VERB_ROOTP_MEMORY_ALLOCATE:
        {
            LibosPrintf("LWOS-Root: LwosRootMemoryAllocate(partition:%lld size:%016llx alignment:%016llx accessMode:%llx)", a0_56, a1, a2, a3);
            LwU64 address = 0;
            LibosStatus status = lwosRootMemoryAllocate(a0_56, &address, a1, a2, a3);
            LibosPrintf("LWOS-Root: status:%d address:%016llx\n", status, address);
            SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN, status), address, 0, 0, 0, callingPartition);
            break;
        }

        case LWOS_VERB_ROOTP_PARTITION_MAP_PHYSICAL:
        {
            LibosPrintf("LWOS-Root: LwosRootPartitionMapPhysical(partition:%lld base:%016llx size:%016llx accessMode:%llx)", a0_56, a1, a2, a3);
            LibosStatus status = lwosRootPartitionMapPhysical(callingPartition, a0_56, a1, a2, a3);
            LibosPrintf("LWOS-Root: status:%d\n", status);
            SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN, status), 0, 0, 0, 0, callingPartition);
            break;
        }

        case LWOS_VERB_ROOTP_PARTITION_INIT:
        {
            LibosPrintf("LWOS-Root: LwosRootPartitionInit(partition:%lld image:%016llx numaBegin:%016llx memorySize:%016llx globalPort:%08llx)", a0_56, a1, a2, a3, a4);
            LibosStatus status = lwosRootPartitionInit(callingPartition, a0_56, a1, a2, a3, a4);
            LibosPrintf("LWOS-Root: status:%d\n", status);
            SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN, status), 0, 0, 0, 0, callingPartition);            
            break;
        }

        case LWOS_VERB_ROOTP_PARTITION_EXIT:
        {
            LibosPrintf("LWOS-Root: LwosRootPartitionExit()\n");
            lwosRootPartitionExit(callingPartition);      
            break;
        }

        case LWOS_VERB_ROOTP_GLOBAL_PORT_ALLOCATE:
        {
            LibosPrintf("LWOS-Root: LwosRootGlobalPortAllocate()");
            LwU32 port = 0;
            LibosStatus status = lwosRootGlobalPortAllocate(callingPartition, &port);
            LibosPrintf("LWOS-Root: status:%d port:%08x\n", status, port);
            SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN, status), port, 0, 0, 0, callingPartition);
            break;
        }

        case LWOS_VERB_ROOTP_GLOBAL_PORT_GRANT:
        {
            LibosPrintf("LWOS-Root: LwosRootGlobalPortGrant(port:%08llx partition:%lld mask:%016llx)", a0_56, a1, a2);
            LibosStatus status = lwosRootGlobalPortGrant(callingPartition, a0_56, a1, a2);
            LibosPrintf("LWOS-Root: status:%d\n", status);
            SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN, status), 0, 0, 0, 0, callingPartition);
            break;
        }
        
        default:
            SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN, LibosErrorAccess), 0, 0, 0, 0, callingPartition);
    }

    SeparationKernelShutdown();
}

static void mmioWrite(LwU64 address, LwU64 value) {
    *(volatile LwU32 *)address = value;
}

static LwU64 mmioRead(LwU64 address) {
    return *(volatile LwU32 *)address;
}

int putc(struct LibosStream * s, char ch) 
{
    mmioWrite(LW_PRGNLCL_FALCON_DEBUGINFO, ch);
    while (mmioRead(LW_PRGNLCL_FALCON_DEBUGINFO))    
        ;
    return 0;
}


void Panic()
{
    LibosPrintf("Panic.\n");
    SeparationKernelShutdown();
}

