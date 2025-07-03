#include "libbootfs.h"
#include "libmem.h"
#include "libos_partition.h"
#include "kernel/lwriscv-2.0/sbi.h"
#include "root/root.h"
#include "peregrine-headers.h"
#include "libprintf.h"
#include <lwmisc.h>

void LwosPartitionStart();
extern char image_offset[];

#   define Panilwnless(cond)                 \
        if (!(cond))                         \
            SeparationKernelShutdown();      \

/*
    @todo: How do you run another partition and then return to where you were?
*/

LwU64 clz(LwU64 x)
{
    if (x == 1) return 63;

    LwU64 i = 0;
    for (; i < 64; i++)
    {
        if ((x & (0x8000000000000000 >> i)) != 0ULL)
        {
            break;
        }
    }

    return i;
}

LwU64 next_power_2(LwU64 x)
{
    if ((x & (x - 1)) == 0)
    {
        return x;
    }

    return (1ULL << (64 - clz(x)));
}

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
    LibosBootFsArchive * image = (LibosBootFsArchive *)(a0 + image_offset);
    
    /*
        Heap will be placed after the image in memory
        Ensure it is 2mb aligned
    */
    LwU64 numaBase = (LwU64)image + image->totalSize;
    numaBase = (numaBase + (1<<21)) &~ ((1 << 21)-1);

    /* 4mb numa node */
    LwU64 numaSize = 1024*1024*4;

    LwU64 pmp_size = next_power_2(numaBase - a0 + numaSize);
    LwU64 pmp_address = LW_ALIGN_DOWN64(a0, pmp_size);

    /* Create the LWOS Root partition */
    SeparationKernelEntrypointSet(LWOS_ROOT_PARTITION, (LwU64)LwosPartitionStart);

    /* Give root partition access to 512mb of FB */
    SeparationKernelPmpMap(
        0, 
        LWOS_ROOT_PARTITION, 
        0, 
        PMP_ACCESS_MODE_READ | PMP_ACCESS_MODE_WRITE | PMP_ACCESS_MODE_EXELWTE, 
        pmp_address,
        pmp_size);

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
    Panilwnless(LibosOk == LwosRootPmpAdvise(0, pmp_address, pmp_size, PMP_ACCESS_MODE_READ | PMP_ACCESS_MODE_WRITE | PMP_ACCESS_MODE_EXELWTE));
    Panilwnless(LibosOk == LwosRootPmpAdvise(1, 0, 512<<20, PMP_ACCESS_MODE_READ | PMP_ACCESS_MODE_WRITE | PMP_ACCESS_MODE_EXELWTE));

    /* Give the root partition a heap to work with */
    Panilwnless(LibosOk == LwosRootMemoryOnline(0, numaBase, numaSize, 0xFFFFFFFFFFFFFFFFULL));

    /* Map INTIO, DMEM, and IMEM into the new partition */
    Panilwnless(LibosOk == LwosRootPartitionMapPhysical(2,  0, 512<<20, PMP_ACCESS_MODE_READ | PMP_ACCESS_MODE_WRITE | PMP_ACCESS_MODE_EXELWTE));

    /* Allocate FB for the new partition */
    LwU64 heapBase, heapSize = 4 << 19;
    Panilwnless(LibosOk == LwosRootMemoryAllocate(2, &heapBase, heapSize, heapSize, PMP_ACCESS_MODE_READ | PMP_ACCESS_MODE_WRITE | PMP_ACCESS_MODE_EXELWTE));

    /* Start running the new partition using the same filesystem */
    LwosRootPartitionInit(2, (LwU64)image, heapBase, heapSize);

    /* Switch to the newly minted partition */
    SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_LIBOS_RUN, 0), 0, 0, 0, 0, 2);
}

