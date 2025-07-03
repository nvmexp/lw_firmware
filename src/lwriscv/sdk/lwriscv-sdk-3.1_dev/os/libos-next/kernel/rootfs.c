#include "kernel.h"
#include "libbootfs.h"
#include "mm/address_space.h"
#include "diagnostics.h"
#include "libos.h"

AddressSpaceRegion * rootFsMapping = 0;
LibosBootFsArchive * rootFs = 0;
LwU64 rootFsPhysical = 0;

LibosStatus KernelRootfsMountFilesystem(LwU64 physicalAddress)
{
    rootFsPhysical = physicalAddress;
    KernelPrintf("Libos: Mounted root filesystem PA=%llx\n", physicalAddress);
    // Use the kernel identity mapping to acess the header
    LibosBootFsArchive * header = (LibosBootFsArchive *)KernelPhysicalIdentityMap(physicalAddress);
    if (header->magic != LIBOS_BOOT_FS_MAGIC)
        return LibosErrorArgument;

    /* Map the entire file */
    rootFsMapping = KernelAddressSpaceAllocate(kernelAddressSpace, 0, header->totalSize);
    if (!rootFsMapping) 
        return LibosErrorOutOfMemory;

    LibosStatus status = KernelAddressSpaceMapContiguous(kernelAddressSpace, rootFsMapping, 0, physicalAddress, header->totalSize, LibosMemoryReadable);
    if (status != LibosOk) {
        KernelAddressSpaceFree(kernelAddressSpace, rootFsMapping);
        return status;
    }

    rootFs = (LibosBootFsArchive *)rootFsMapping->start;

    // Enumerate files in an archive
    LibosBootfsRecord * file = LibosBootFileFirst(rootFs);
    while (file) {
        KernelPrintf("Libos: Mounted file %s\n", LibosBootFileName(rootFs, file));
        file = LibosBootFileNext(rootFs, file);
    }

    return LibosOk;
}

LibosBootfsRecord * KernelFileOpen(const char * name)
{
    LibosBootfsRecord * file = LibosBootFileFirst(rootFs);

    while (file) {
        if (strcmp(name, LibosBootFileName(rootFs, file)) == 0)
            return file;
        file = LibosBootFileNext(rootFs, file);
    }

    return 0;
}

LwU64 KernelFilePhysical(LibosBootfsExelwtableMapping * file)
{
    return rootFsPhysical + file->offset;
}
