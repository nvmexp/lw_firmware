#pragma once

#include "libos_status.h"

#include <lwtypes.h>
#include "kernel/lwriscv-2.0/sbi.h"

#define LWOS_ROOT_PARTITION                                 1

// Common verbs
#define LWOS_VERB_RETURN                                    0x10ULL

// Verbs supported by root partition
#define LWOS_VERB_ROOTP_INITIALIZE                          0x20ULL
#define LWOS_VERB_ROOTP_INITIALIZE_ADVISE_PMP               0x21ULL 
#define LWOS_VERB_ROOTP_MEMORY_ONLINE                       0x22ULL 
#define LWOS_VERB_ROOTP_MEMORY_ALLOCATE                     0x23ULL 
#define LWOS_VERB_ROOTP_PARTITION_MAP_PHYSICAL              0x24ULL
#define LWOS_VERB_ROOTP_PARTITION_INIT                      0x25ULL 
#define LWOS_VERB_ROOTP_INITIALIZE_COMPLETED                0x26ULL 
#define LWOS_VERB_ROOTP_GLOBAL_PORT_ALLOCATE                0x27ULL
#define LWOS_VERB_ROOTP_GLOBAL_PORT_GRANT                   0x28ULL
#define LWOS_VERB_ROOTP_PARTITION_EXIT                      0x29ULL
#define LWOS_VERB_ROOTP_PARTITION_EXIT_HANDLE_COMPLETE      0x2AULL

// Implemented by LIBOS partitions
#define LWOS_VERB_LIBOS_INIT                                0x30ULL 
#define LWOS_VERB_LIBOS_RUN                                 0x31ULL
#define LWOS_VERB_LIBOS_GLOBAL_MESSAGE_PERFORM_SEND         0x32ULL
#define LWOS_VERB_LIBOS_GLOBAL_MESSAGE_PERFORM_RECV         0x33ULL
#define LWOS_VERB_LIBOS_PARTITION_EXIT_NOTIFICATION         0x34ULL

#define LWOS_ROOTP_GLOBAL_PORT_MAX                          256
#define LWOS_ROOTP_GLOBAL_MESSAGE_MAX_SIZE                  4096
#define LWOS_ROOTP_GLOBAL_MESSAGE_ATTACHED_PORT_MAX_COUNT   8

typedef LwU8 LwosGlobalPortGrant;
#define LWOS_ROOTP_GLOBAL_PORT_GRANT_SEND                   1
#define LWOS_ROOTP_GLOBAL_PORT_GRANT_RECV                   2

#define LWOS_ROOTP_GLOBAL_PORT_NONE                         0xFFFFFFFFULL

#define LWOS_ROOTP_GLOBAL_PORT_OWNER(x)                     (x >> 24)
#define LWOS_ROOTP_GLOBAL_PORT_SEQ(x)                       (x & 0x00FFFFFF)
#define LWOS_ROOTP_GLOBAL_PORT_BUILD(owner, seq)            ((owner << 24) | seq)


LibosStatus LwosRootInitialize();
LibosStatus LwosRootPmpAdvise(LwU64 localEntry, LwU64 begin, LwU64 lengthPower2, LwU32 pmpAccessMode);

LibosStatus LwosRootMemoryOnline(LwU8 node, LwU64 begin, LwU64 size, LwU64 partitionAllowedAllocatorsMask);
LibosStatus LwosRootMemoryAllocate(LwU32 partitionOwner, LwU64 * outAddress, LwU64 size, LwU64 alignment, LwU32 accessMode);

LibosStatus LwosRootPartitionMapPhysical(LwU64 partition, LwU64 base, LwU64 size, LwU32 accessMode);
LibosStatus LwosRootPartitionInit(LwU64 partition, LwU64 image, LwU64 numaBegin, LwU64 memorySize, LwU32 globalPort);
__attribute__((noreturn)) void LwosRootPartitionExit();

LibosStatus LwosRootGlobalPortAllocate(LwU32 *port);
LibosStatus LwosRootGlobalPortGrant(LwU32 port, LwU64 partition, LwosGlobalPortGrant mask);


#define LWOS_A0_VERB(arg)  ((arg) >> 56)
#define LWOS_A0_ARG(arg)  ((arg) & ((1ULL << 56)-1))
#define LWOS_A0(verb,arg)  (((verb) << 56) | (arg))

#define GLOBAL_PAGE_SIZE 16384

struct GlobalPage
{
    union
    {
        // These are overflowed arguments that don't fit in the registers
        // Used when verb is LWOS_VERB_LIBOS_INIT
        struct LibosInit
        {
            LwU8 partitionId;
            LwU64 imageStart;
            LwU64 libosEntrypoint;
            LwU32 parentGlobalPort;
        } libosInit;

        struct GlobalMessage
        {
            LwU32 toGlobalPort;
            LwU8 message[LWOS_ROOTP_GLOBAL_MESSAGE_MAX_SIZE];
            LwU32 size;
        } globalMessage;
    } args;
 
    LwU64 initializedPartitionMask;

    struct GlobalPortStatus
    {
        LwU64 partitionsPendingSend;
        LwU64 partitionsPendingRecv;
    } globalPortStatuses[LWOS_ROOTP_GLOBAL_PORT_MAX];

    // TODO: WARNING!!! We can't ship like this!!! Move this to read-only page.
    struct GlobalPortGrant
    {
        LwU64 partitionsGrantSend;
        LwU64 partitionsGrantRecv;
    } globalPortGrants[LWOS_ROOTP_GLOBAL_PORT_MAX];
};
