#include "libbootfs.h"
#include "libos.h"
#include "kernel/lwriscv-2.0/sbi.h"
#include "root/root.h"
#include "peregrine-headers.h"
#include "libprintf.h"

void LwosPartitionStart();

#   define Panilwnless(cond)                 \
        if (!(cond))                         \
            SeparationKernelShutdown();      \

/*
    @todo: How do you run another partition and then return to where you were?
*/

__attribute__((used)) void InitPartitionEntry(LwU64 a0, LwU64 a1, LwU64 a2, LwU64 a3, LwU64 a4)
{
    /* Protection against re-entrancy */
    static LwBool firstBoot = LW_TRUE;

    if (!firstBoot)
        SeparationKernelShutdown();
    
    firstBoot = LW_FALSE;

    /*
        The image address was computed by start.s 
    */
    LibosBootFsArchive * image = (LibosBootFsArchive *)a0;
    
    /*
        Heap will be placed after the image in memory
        Ensure it is 2mb aligned
    */
    LwU64 numaBase = (LwU64)image + image->totalSize;
    numaBase = (numaBase + (1<<21)) &~ ((1 << 21)-1);

    /* 256mb numa node */
    LwU64 memorySize = 1024 << 20;

    /* Create the LWOS Root partition */
    SeparationKernelEntrypointSet(LWOS_ROOT_PARTITION, (LwU64)LwosPartitionStart);

    /* Give root partition access to 512mb of FB */
    SeparationKernelPmpMap(
        0, 
        LWOS_ROOT_PARTITION, 
        0, 
        PMP_ACCESS_MODE_READ | PMP_ACCESS_MODE_WRITE | PMP_ACCESS_MODE_EXELWTE, 
        LW_RISCV_AMAP_FBGPA_START,
        memorySize);
    
    /* Give root partition access to INTIO */
    SeparationKernelPmpMap(
        0, 
        LWOS_ROOT_PARTITION, 
        1, 
        PMP_ACCESS_MODE_READ | PMP_ACCESS_MODE_WRITE | PMP_ACCESS_MODE_EXELWTE, 
        0,
        512<<20);

    /* Initialize the root LWOS Root partition */
    LwosRootInitialize();

    /* Tell the root partition the contents of it's PMP entries */
    Panilwnless(LibosOk == LwosRootPmpAdvise(0, LW_RISCV_AMAP_FBGPA_START, memorySize, PMP_ACCESS_MODE_READ | PMP_ACCESS_MODE_WRITE | PMP_ACCESS_MODE_EXELWTE));
    Panilwnless(LibosOk == LwosRootPmpAdvise(1, 0, 512<<20, PMP_ACCESS_MODE_READ | PMP_ACCESS_MODE_WRITE | PMP_ACCESS_MODE_EXELWTE));

    /* Give the root partition a heap to work with */
    Panilwnless(LibosOk == LwosRootMemoryOnline(0, numaBase, memorySize, 0xFFFFFFFFFFFFFFFFULL));

    /* Map INTIO, DMEM, and IMEM into the new partition */
    Panilwnless(LibosOk == LwosRootPartitionMapPhysical(2,  0, 512<<20, PMP_ACCESS_MODE_READ | PMP_ACCESS_MODE_WRITE | PMP_ACCESS_MODE_EXELWTE));

    /* Allocate FB for the new partition */
    LwU64 heapBase, heapSize = 128 << 20;
    Panilwnless(LibosOk == LwosRootMemoryAllocate(2, &heapBase, heapSize, heapSize, PMP_ACCESS_MODE_READ | PMP_ACCESS_MODE_WRITE | PMP_ACCESS_MODE_EXELWTE));

    /* Start running the new partition using the same filesystem */
    LwosRootPartitionInit(2, (LwU64)image, heapBase, heapSize, LWOS_ROOTP_GLOBAL_PORT_NONE);

    /* Switch to the newly minted partition */
    SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_LIBOS_RUN, 0), 0, 0, 0, 0, 2);
}

