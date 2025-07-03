#include "kernel.h"
#include "libos.h"
#include "lwtypes.h"
#include "libbit.h"
#include "mm/objectpool.h"
#include "rootfs.h"
#include "mm/address_space.h"
#include "task.h"
#include "scheduler.h"
#include "libriscv.h"
#include "loader.h"
#include "diagnostics.h"
#include "rootfs.h"
#include "mm/memorypool.h"
#include "lwriscv-2.0/sbi.h"

typedef struct {
    // AddressSpace::mappedElfRegions indexes the loaded elfs
    // and holds a reference to mappedElf.
    LibosTreeHeader                header;

    LibosBootfsExelwtableMapping * mapping;
    LibosBootfsRecord            * elf;

    // We own this allocation
    AddressSpaceRegion           * allocation;
} MappedElf;

void KernelMappedElfDestroy(void * ptr) {
    MappedElf * mapping = ptr;
    //KernelAssert(ListUnlinked(&mapping->header));
    KernelPoolRelease(mapping->allocation);
}

void KernelElfDestroy(void * ptr) {
}

void KernelInitElfLoader()
{
    KernelPoolConstruct(sizeof(MappedElf), LIBOS_POOL_MAPPED_ELF, KernelMappedElfDestroy);
    KernelPoolConstruct(sizeof(Elf), LIBOS_POOL_ELF, KernelElfDestroy );
}

LibosStatus KernelElfOpen(const char * elfName, Elf * * outElf)
{
    // Find the file in the rootfs
    LibosBootfsRecord * elfRecord = KernelFileOpen(elfName);

    if (!elfRecord)
        return LibosErrorFailed;

    // Create the elf handle
    Elf * elf;
    elf = (Elf *)KernelPoolAllocate(LIBOS_POOL_ELF);
    if (!elf)
        return LibosErrorOutOfMemory;  

    // Fill out elf structure
    elf->elf = elfRecord;

    *outElf = elf;
    
    return LibosOk;         
}


// Declare a map from [AddressSpace, LibosBootfsExelwtableMapping]  -> MappedElf
// @todo: If we don't port to C++, this needs to be made reference counting aware.
LIBOS_TREE_SIMPLE(AddressSpace, mappedElfRegions, MappedElf, mapping, LibosBootfsExelwtableMapping *);

LibosStatus KernelElfMap(BORROWED_PTR AddressSpace * asid, LibosBootfsRecord * elfRecord)
{
    // Loop over mappings in ELF
    LibosBootfsExelwtableMapping * mapping = LibosBootMappingFirst(rootFs, elfRecord);
    while (mapping) 
    {
        // Does this task already have this mapping?
        // @note: Safe to borrow since caller has a reference count to the AddressSpace
        //        and mappings cannot be removed.
        BORROWED_PTR MappedElf * previousMapping = AddressSpaceFindMappedElf(asid, mapping);

        if (previousMapping)
            continue;

        KernelPrintf("Libos: Loader mapping %s:%llx into asid=%llx.\n", LibosBootFileName(rootFs, elfRecord), (LwU64)mapping->virtualAddress, (LwU64)asid);  

        // Allocate the tracking record
        OWNED_PTR MappedElf * mappedElf;
        mappedElf = (MappedElf *)KernelPoolAllocate(LIBOS_POOL_MAPPED_ELF);
        if (!mappedElf)
            return LibosErrorIncomplete;  

        // Allocate the virtual address range in the exelwtable
        OWNED_PTR AddressSpaceRegion * allocation;
        allocation = KernelAddressSpaceAllocate(asid, mapping->virtualAddress, mapping->length);

        if (!allocation) {
            KernelPoolRelease(mappedElf);
            return LibosErrorIncomplete;
        }

        // Translate elf mapping flags to PAGETABLE mapping flags
        LwU64 flags = 0;
        if (mapping->attributes & LIBOS_BOOTFS_MAPPING_X) 
            flags |= LibosMemoryExelwtable;

        if (mapping->attributes & LIBOS_BOOTFS_MAPPING_R) 
            flags |= LibosMemoryReadable;

        if (mapping->attributes & LIBOS_BOOTFS_MAPPING_W) 
            flags |= LibosMemoryWriteable;

        // Map the memory
        if (flags & LibosMemoryWriteable)
        {
            for (LwU64 i = 0; i < mapping->length; i+=4096)
            {
                // @todo: Why aren't we using a kernel mapping here?
                void * source = LibosBootMappingBuffer(rootFs, mapping) + i;
                
                // Allocate a page
                LwU64 pageAddress;
                if (LibosOk != KernelMemoryPoolAllocatePage(KernelPoolMemorySet, 12, &pageAddress))
                    return LibosErrorIncomplete;

                // Map the page into kernel
                void * dest = KernelMapPhysicalAligned(2, pageAddress, 4096); 

                // Copy
                memcpy(dest, source, 4096);

                // Map into target asid
                LibosStatus status = KernelAddressSpaceMapContiguous(
                    asid, 
                    allocation,
                    i,
                    pageAddress, 
                    4096, 
                    flags);      

                if (status != LibosOk)
                {
                    KernelAddressSpaceFree(asid, allocation);
                    KernelPoolRelease(mappedElf);
                    return LibosErrorIncomplete;
                }                            
            }
        }
        else
        {
            // @todo: Bug bug we need to copy the writeable pages now.
            LibosStatus status = KernelAddressSpaceMapContiguous(
                asid, 
                allocation,
                0,
                KernelFilePhysical(mapping), 
                mapping->length, 
                flags);       

            if (status != LibosOk)
            {
                KernelAddressSpaceFree(asid, allocation);
                KernelPoolRelease(mappedElf);
                return LibosErrorIncomplete;
            }                     
        }

        // Insert the mapping structure into the address space
        // @note: This insertion function isn't reference counting aware
        //        Don't increment/decrement the reference count
        AddressSpaceInsertMappedElf(asid, mappedElf);
        mappedElf->allocation = allocation;

        mapping = LibosBootMappingNext(rootFs, mapping);
    }

    return LibosOk;
}