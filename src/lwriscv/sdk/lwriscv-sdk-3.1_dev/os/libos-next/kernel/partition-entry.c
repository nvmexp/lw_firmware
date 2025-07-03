#include "libbootfs.h"
#include "libos.h"
#include "diagnostics.h"

typedef void (*PKernelInit)(
    LwU64 initArg0, 
    LwU64 initArg1, 
    LwU64 initArg2, 
    LwU64 initArg3, 
    LwU64 memoryStart, 
    LwU64 memorySize,
    LwU64 kernelDataStart,
    LwU64 imageStart);

// @todo: The first partition to activate us is the object manager
LwU64 objectManagerPartition = -1ULL;

__attribute__((used)) void RootPartitionEntry(LwU64 a0_reason, LwU64 a1, LwU64 a2, LwU64 a3, LwU64 a4, LwU64 a5_from_partition)
{
    if (a0_reason == LIBOS_PARTITION_CODE_BOOT && objectManagerPartition == -1ULL) 
    {
        // First boot
        objectManagerPartition =  a5_from_partition;

        LwU64 heapBase   = a1;
        LwU64 heapSize   = a2;
        LibosBootFsArchive * imageBase = (LibosBootFsArchive *)a3;

        LibosBootfsRecord * kernelImage = LibosBootFileFirst(imageBase);

        LibosBootfsExelwtableMapping * text = LibosBootFindMappingText(imageBase, kernelImage);
        LibosBootfsExelwtableMapping * data = LibosBootFindMappingData(imageBase, kernelImage);

        LwU64 dataSection = heapBase;
        memcpy((void *)heapBase, (void *)((LwU64)imageBase + data->offset), data->length);

        heapBase += data->length;
        heapSize -= data->length;

        LibosElf64Header * elf = LibosBootFindElfHeader(imageBase, kernelImage);

        LwU64 a = elf->entry - text->virtualAddress + text->offset + (LwU64)imageBase;
        ((PKernelInit)a)(a0_reason, a1, a2, a3, heapBase, heapSize, dataSection, (LwU64)imageBase);
    }
}
