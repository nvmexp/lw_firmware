#include "libos.h"
#include "libprintf.h"
#include "peregrine-headers.h"
#include "libos_xcsr.h"
#include "libriscv.h"

LwU64 mmio = 0x400000;

void putc(char ch) {
  *(volatile LwU32 *)(mmio + LW_PRGNLCL_FALCON_DEBUGINFO - LW_RISCV_AMAP_INTIO_START) = ch;
  while (*(volatile LwU32 *)(mmio + LW_PRGNLCL_FALCON_DEBUGINFO - LW_RISCV_AMAP_INTIO_START))
    ;
}

LibosStatus spawnPartition(LwU8 partitionId, LwU64 memorySize, LibosPartitionHandle *partition)
{
    LibosStatus status = LibosPartitionCreate(partitionId, partition);
    if (status != LibosOk)
    {
        return status;
    }

    // Grant IMEM/DMEM/INTIO to the new partition
    status = LibosPartitionMemoryGrantPhysical(*partition, 0, 512 << 20);
    if (status != LibosOk)
    {
        return status;
    }

    LwU64 numaBase;
    status = LibosPartitionMemoryAllocate(*partition, &numaBase, memorySize, memorySize);
    if (status != LibosOk)
    {
        return status;
    }

    status = LibosPartitionSpawn(*partition, numaBase, memorySize);
    return status;
}

struct InitMessage {
    LIBOS_MESSAGE_HEADER(handles, handleCount, 2);
};

LibosStatus testTransparentMessagingParent(LibosPartitionHandle childPartition)
{
    LibosStatus status;

    LibosPrintf("PPP1\n");

    /**
     * @brief Send the child partition a pair of ports for future communication
     * 
     */
    LibosPortHandle recvPort, sendPort;
    status = LibosPortCreate(&recvPort);
    LibosAssert(status == LibosOk);
    status = LibosPortCreate(&sendPort);
    LibosAssert(status == LibosOk);

    LibosPrintf("PPP2\n");
    struct InitMessage initMessage;
    initMessage.handleCount = 2;
    initMessage.handles[0].grant = LibosGrantPortAll;  // grant recv
    initMessage.handles[0].handle = sendPort;
    initMessage.handles[1].grant = LibosGrantPortAll;  // grant send
    initMessage.handles[1].handle = recvPort;

    status = LibosPortSyncSend(childPartition,  &initMessage, sizeof(initMessage), 0, LibosTimeoutInfinite, LibosPortFlagTransferHandles);
    LibosAssert(status == LibosOk);
    LibosPrintf("PPP3\n");

    /**
     * @brief Let's send 
     * 
     */

    char helloMessage[] = "Hello from the parent partition.\n";
    char childMessage[512];
    status = LibosPortSyncSendRecv(
        sendPort,  helloMessage, sizeof(helloMessage),
        recvPort,  childMessage, sizeof(childMessage),
        0, LibosTimeoutInfinite, 0
    );
    LibosAssert(status == LibosOk);
    LibosPrintf("Child partition has replied to our greeting: %s\n", childMessage);

    return LibosOk;
}

LibosStatus testTransparentMessagingChild(LibosPortHandle KernelPartitionParentPort)
{
    struct InitMessage message;

    // Let's get the initialization message
    LibosStatus status = LibosPortSyncRecv(KernelPartitionParentPort, &message, sizeof(message), 0, LibosTimeoutInfinite, LibosPortFlagTransferHandles);
    LibosAssert(status == LibosOk);
    LibosAssert(message.handleCount == 2);

    // Extract the handles from our parent partition
    LibosPortHandle recvPort = message.handles[0].handle;
    LibosPortHandle sendPort = message.handles[1].handle;

    LibosPrintf("Child partition received init message from parent: %d %d\n", sendPort, recvPort);

    // Wait for hello message from parent
    char recvBuf[128];
    status = LibosPortSyncRecv(recvPort,  recvBuf, sizeof(recvBuf), 0, LibosTimeoutInfinite,0);
    LibosAssert(status == LibosOk);
    LibosPrintf("Child partition received greeting from parent: %s\n", recvBuf);

    // Send the reply
    char greetingResponse[] = "Yas! Transparent messaging works!";
    status = LibosPortSyncSend(sendPort,  greetingResponse, sizeof(greetingResponse), 0, LibosTimeoutInfinite,0);
    LibosAssert(status == LibosOk);

    // @todo: Test port lost

    return LibosOk;
}

__attribute__((used)) int main(LwU8 selfPartitionId, LibosPortHandle KernelPartitionParentPort)
{
    LibosRegionHandle region;
    LibosAssert(LibosOk == LibosAddressSpaceMapPhysical(
        LibosDefaultAddressSpace, 
        mmio, 
        LW_RISCV_AMAP_INTIO_START, 
        LW_RISCV_AMAP_INTIO_END - LW_RISCV_AMAP_INTIO_START, 
        LibosMemoryReadable | LibosMemoryWriteable,
        &region
    ));


    LibosPrintf("Partition %u has reached init.\n", selfPartitionId);

    if (KernelPartitionParentPort == LibosPortHandleNone)
        LibosPrintf("This is the first libos partition.\n");
    else
        LibosPrintf("We are a chld of another partition. Parent port: %u.\n", KernelPartitionParentPort);

    // Create address space for worker task
    LibosAddressSpaceHandle workerAsid = 0;
    LibosAssert(LibosOk == LibosAddressSpaceCreate(&workerAsid));
    LibosPrintf("Created address space: %x\n", workerAsid);

    // Open the worker task elf
    static LibosElfHandle workerElf = 0;
    LibosAssert(LibosOk == LibosElfOpen("worker.elf", &workerElf));

    // Inject ELF into address space
    LwU64 entry = 0;
    LibosAssert(LibosOk == LibosElfMap(workerAsid, workerElf, &entry));

    // Map PRI into target
    LibosRegionHandle region2;
    LibosAssert(LibosOk == LibosAddressSpaceMapPhysical(
        workerAsid, 
        0x400000, 
        LW_RISCV_AMAP_INTIO_START, 
        LW_RISCV_AMAP_INTIO_END - LW_RISCV_AMAP_INTIO_START, 
        LibosMemoryReadable | LibosMemoryWriteable,
        &region2
    ));    

    LibosTaskHandle child;       
    LibosAssert(LibosOk == LibosTaskCreate(
        &child,
        LIBOS_PRIORITY_NORMAL, 
        workerAsid,
        LibosDefaultMemorySet,
        entry
    ));
    LibosPrintf("Child task created %x.\n", child);

    // Let's send our child task an init message with a pair of ports for
    // future communication.
    // @note: This really should be part of the task creation call
    LibosPortHandle recvPort, sendPort;
    LibosStatus status = LibosPortCreate(&recvPort);
    LibosAssert(status == LibosOk);
    status = LibosPortCreate(&sendPort);
    LibosAssert(status == LibosOk);

    LibosPrintf("Sending port handles to child task.\n");
    struct InitMessage initMessage;
    initMessage.handleCount = 2;
    initMessage.handles[0].grant = LibosGrantPortAll;  // grant recv
    initMessage.handles[0].handle = recvPort;
    initMessage.handles[1].grant = LibosGrantPortAll;  // grant send
    initMessage.handles[1].handle = sendPort;
    LibosAssert(LibosOk == LibosPortSyncSend(child, &initMessage, sizeof(initMessage), 0, 0, LibosPortFlagTransferHandles));


    // Send a message
    LibosPortSyncSend(sendPort, 0, 0, 0, LibosTimeoutInfinite, 0);

    // Wait on the child task for trap/termination
    LibosTaskState childTaskState;
    LibosAssert(LibosOk == LibosPortSyncRecv(child, &childTaskState, sizeof(childTaskState), 0, LibosTimeoutInfinite, 0));

    if (childTaskState.xcause == LW_RISCV_CSR_SCAUSE_EXCODE_UCALL && childTaskState.registers[RISCV_T0] == LIBOS_SYSCALL_TASK_EXIT)
        LibosPrintf("Child task exited normally.\n");

    LibosPartitionHandle partition;
    if (selfPartitionId == 2)
    {
        LwU64 memorySize = 128 << 20;
        LibosStatus status = spawnPartition(selfPartitionId + 1, memorySize, &partition);
        if (status == LibosOk)
        {
            LibosPrintf("Successfully spawned partition %u, handle %u.\n", selfPartitionId + 1, partition);
        }
        else
        {
            LibosPrintf("Failed to spawn partition %u: %d.\n", selfPartitionId + 1, status);
            LibosProcessorShutdown();
        }
    }

    if (selfPartitionId == 2)
    {
        if (testTransparentMessagingParent(partition) == LibosOk)
        {
            LibosPrintf("Parent: transparent messaging test passed!\n");
        }
        else
        {
            LibosPrintf("Parent: transparent messaging test failed!\n");
            LibosProcessorShutdown();
        }
    }
    else
    {
        if (testTransparentMessagingChild(KernelPartitionParentPort) == LibosOk)
        {
            LibosPrintf("Child: transparent messaging test passed!\n");
        }
        else
        {
            LibosPrintf("Child: transparent messaging test failed!\n");
            LibosProcessorShutdown();
        }
    }

    LibosPartitionExit();

    return 0;
}




