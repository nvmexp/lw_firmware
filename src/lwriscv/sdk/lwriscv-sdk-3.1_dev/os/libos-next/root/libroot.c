#include "root.h"
#include "libos_status.h"
#include "kernel/lwriscv-2.0/sbi.h"
#include "libbit.h"


LibosStatus LwosRootMemoryOnline(LwU8 node, LwU64 begin, LwU64 size, LwU64 partitionAllowedAllocatorsMask)
{
    LwU64 arguments[5] = {
        LWOS_A0(LWOS_VERB_ROOTP_MEMORY_ONLINE, node), begin, size, partitionAllowedAllocatorsMask, 0
    };
    LibosStatus status = SeparationKernelPartitionCall(LWOS_ROOT_PARTITION,  arguments);
    if (status!=LibosOk)
        return status;
    return (LibosStatus)LWOS_A0_ARG(arguments[0]);
}

LibosStatus LwosRootPmpAdvise(LwU64 localEntry, LwU64 begin, LwU64 lengthPower2, LwU32 pmpAccessMode)
{
    LwU64 arguments[5] = {
        LWOS_A0(LWOS_VERB_ROOTP_INITIALIZE_ADVISE_PMP, localEntry), begin, lengthPower2, pmpAccessMode, 0
    };
    LibosStatus status = SeparationKernelPartitionCall(LWOS_ROOT_PARTITION,  arguments);
    if (status!=LibosOk)
        return status;
    return (LibosStatus)LWOS_A0_ARG(arguments[0]);
}

LibosStatus LwosRootMemoryAllocate(LwU32 partitionOwner, LwU64 * outAddress, LwU64 size, LwU64 alignment, LwU32 accessMode)
{
    LwU64 arguments[5] = {
        LWOS_A0(LWOS_VERB_ROOTP_MEMORY_ALLOCATE, partitionOwner), size, alignment, accessMode, 0
    };
    LibosStatus status = SeparationKernelPartitionCall(LWOS_ROOT_PARTITION,  arguments);
    if (status!=LibosOk)
        return status;
    *outAddress = arguments[1];
    return (LibosStatus)LWOS_A0_ARG(arguments[0]);
}

LibosStatus LwosRootPartitionMapPhysical(LwU64 partition, LwU64 base, LwU64 size, LwU32 accessMode)
{
    LwU64 arguments[5] = {
        LWOS_A0(LWOS_VERB_ROOTP_PARTITION_MAP_PHYSICAL, partition), base, size, accessMode, 0
    };
    LibosStatus status = SeparationKernelPartitionCall(LWOS_ROOT_PARTITION,  arguments);
    if (status!=LibosOk)
        return status;
    return (LibosStatus)LWOS_A0_ARG(arguments[0]);
}

LibosStatus LwosRootPartitionInit(LwU64 partition, LwU64 image, LwU64 numaBegin, LwU64 memorySize, LwU32 globalPort)
{
    LwU64 arguments[5] = {
        LWOS_A0(LWOS_VERB_ROOTP_PARTITION_INIT, partition), image, numaBegin, memorySize, globalPort
    };
    LibosStatus status = SeparationKernelPartitionCall(LWOS_ROOT_PARTITION,  arguments);
    if (status!=LibosOk)
        return status;
    return (LibosStatus)LWOS_A0_ARG(arguments[0]);
}

__attribute__((noreturn)) void LwosRootPartitionExit()
{
    SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_ROOTP_PARTITION_EXIT, 0), 0, 0, 0, 0, LWOS_ROOT_PARTITION);
}


LibosStatus LwosRootInitialize()
{
    LwU64 arguments[5] = {
        LWOS_A0(LWOS_VERB_ROOTP_INITIALIZE, 0),0,0,0,0
    };
    LibosStatus status = SeparationKernelPartitionCall(LWOS_ROOT_PARTITION,  arguments);
    if (status!=LibosOk)
        return status;    
    return (LibosStatus)LWOS_A0_ARG(arguments[0]);
}

LibosStatus LwosRootGlobalPortAllocate(LwU32 *port)
{
    LwU64 arguments[5] = {
        LWOS_A0(LWOS_VERB_ROOTP_GLOBAL_PORT_ALLOCATE, 0),0,0,0,0
    };
    LibosStatus status = SeparationKernelPartitionCall(LWOS_ROOT_PARTITION,  arguments);
    if (status!=LibosOk)
        return status;
    *port = arguments[1];
    return (LibosStatus)LWOS_A0_ARG(arguments[0]);
}

LibosStatus LwosRootGlobalPortGrant(LwU32 port, LwU64 partition, LwosGlobalPortGrant mask)
{
    LwU64 arguments[5] = {
        LWOS_A0(LWOS_VERB_ROOTP_GLOBAL_PORT_GRANT, port), partition, mask, 0, 0
    };
    LibosStatus status = SeparationKernelPartitionCall(LWOS_ROOT_PARTITION,  arguments);
    if (status!=LibosOk)
        return status;
    return (LibosStatus)LWOS_A0_ARG(arguments[0]);
}
