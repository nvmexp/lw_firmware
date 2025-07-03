#include "libos.h"
#include "libprintf.h"
#include "peregrine-headers.h"
#include "libos_xcsr.h"
#include "libriscv.h"
#include "libos_ipc.h"

#include <liblwriscv/print.h>

int main(LwU8 selfPartitionId);

LwU64 mmio = 0x400000;

LibosStatus spawnPartition(LwU8 partitionId, LwU64 numaSize, LibosPartitionHandle *partition)
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
    status = LibosPartitionMemoryAllocate(*partition, &numaBase, numaSize, numaSize);
    if (status != LibosOk)
    {
        return status;
    }

    status = LibosPartitionSpawn(*partition, numaBase, numaSize);
    return status;
}

static void writeMailboxForMods()
{
    volatile LwU32* mailbox0 = (volatile LwU32*)(mmio + LW_PRGNLCL_FALCON_MAILBOX0 - LW_RISCV_AMAP_INTIO_START);
    volatile LwU32* mailbox1 = (volatile LwU32*)(mmio + LW_PRGNLCL_FALCON_MAILBOX1 - LW_RISCV_AMAP_INTIO_START);
    *mailbox0 = 0xdeadbeef;
    *mailbox1 = 0xcafebabe;
}

static void initPrint(void)
{
    volatile LwU32 dmemSize = *(volatile LwU32*)(mmio + LW_PRGNLCL_FALCON_HWCFG3 - LW_RISCV_AMAP_INTIO_START);
    dmemSize = DRF_VAL(_PRGNLCL, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE, dmemSize) << 8;

    LwU64 printSize = 0x1000;
    LwU64 printStart = LW_RISCV_AMAP_DMEM_START + dmemSize - printSize;

    LibosAddressSpaceMapPhysical(
        ASID_ID_SELF,
        printStart,
        printStart,
        printSize,
        LIBOS_PAGETABLE_LOAD | LIBOS_PAGETABLE_STORE | LIBOS_PAGETABLE_UNCACHED
    );

    printInit((LwU8*)printStart, printSize, 0, 0);
}

__attribute__((used)) int main(LwU8 selfPartitionId)
{
    LibosAddressSpaceMapPhysical(
        ASID_ID_SELF, 
        mmio, 
        LW_RISCV_AMAP_INTIO_START, 
        LW_RISCV_AMAP_INTIO_END - LW_RISCV_AMAP_INTIO_START, 
        LIBOS_PAGETABLE_LOAD | LIBOS_PAGETABLE_STORE
    );

    initPrint();
    printf("Our partition ID is %u.\n", selfPartitionId);


    printf("Init task spawning worker task in same ASID.\n");

    // Create address space for worker task
    LibosAddressSpaceHandle workerAsid = 0;
    LibosAddresSpaceCreate(&workerAsid);

    // Open the worker task elf
    static LibosElfHandle workerElf = 0;
    LibosElfOpen("worker.elf", &workerElf);

    // Inject ELF into address space
    LwU64 entry;
    LibosElfMap(workerAsid, workerElf, &entry);

    // Map PRI into target
    LibosAddressSpaceMapPhysical(
        workerAsid, 
        0x400000, 
        LW_RISCV_AMAP_INTIO_START, 
        LW_RISCV_AMAP_INTIO_END - LW_RISCV_AMAP_INTIO_START, 
        LIBOS_PAGETABLE_LOAD | LIBOS_PAGETABLE_STORE
    );    

    LibosTaskHandle child;       
    (void)LibosTaskCreate(
        &child,
        LIBOS_PRIORITY_NORMAL, 
        workerAsid,
        entry
    );
    printf("Child task created %x.\n", child);

    // Wake up the task
    LibosMessage simple;
    simple.interface = 1;
    simple.message = 0x4E110;

    LibosPortSendAttachObject(SHUTTLE_SYNC_SEND, child, LibosGrantTaskAll);
    LibosPortSyncSend(child, &simple, sizeof(simple), 0, 0);
    printf("Child task message sent.\n");

    // Wait on the child task for trap/termination
    LibosTaskState childTaskState;
    LibosPortSyncRecv(child, (LibosMessage *)&childTaskState, sizeof(childTaskState), 0, LibosTimeoutInfinite);

    if (childTaskState.xcause == LW_RISCV_CSR_SCAUSE_EXCODE_UCALL && childTaskState.registers[RISCV_T0] == LIBOS_SYSCALL_TASK_EXIT)
        printf("Child task exited normally.\n");

    LwU64 numaSize = 4 << 19;
    LibosPartitionHandle partition;

    LibosStatus status = spawnPartition(selfPartitionId + 1, numaSize, &partition);
    if (status == LibosOk)
    {
        printf("Successfully spawned partition %u, handle %u.\n", selfPartitionId + 1, partition);
    }
    else
    {
        printf("Failed to spawn partition %u: %d.\n", selfPartitionId + 1, status);
    }

    writeMailboxForMods();
    LibosProcessorShutdown();

    return 0;
}




