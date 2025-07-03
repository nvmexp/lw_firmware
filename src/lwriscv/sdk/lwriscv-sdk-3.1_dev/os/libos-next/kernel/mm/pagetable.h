#pragma once
#include <libos.h>

typedef struct PageTableGlobal
{
    struct PageTableMiddle * entries[512];
} PageTableGlobal;

typedef struct {
    LwU64 desiredMpuIndex;      // The MPU entry to demand fault this into, 
                                // select 0 for random
    LwU64 virtualAddress;
    LwU64 physicalAddress;
    LwU64 size;
    LwU64 attributes;                              
} VirtualMpuEntry;

typedef LwU64 LibosMemoryAccess;

// Temporary structure returned by PageTableLookup
typedef struct { 
    // Page description
    LwU64             virtualAddress;
    LwU64             size;

    // Translation
    LibosMemoryAccess access;
    LwU64             physicalAddress;

    // Additional data if this is a virtual MPU entry
    VirtualMpuEntry * virtualMpu; 
} PageMapping;

LwBool KernelPageMappingPin(PageTableGlobal * table, LwU64 virtualAddress, /* OUT */ PageMapping * pageMapping);

/**
 * @brief Releases any reference counts held by a PageMapping structure.
 * 
 * @param pageMapping
 */
void              KernelPageMappingRelease(PageMapping * pageMapping);

PageTableGlobal * KernelPagetableAllocateGlobal();
void              KernelPagetableDestroy(PageTableGlobal * global);
LibosStatus       KernelPagetableMapPage(PageTableGlobal * table, LwU64 virtualAddress, LwU64 physicalAddress, LwU64 size, LwU64 access);
void            * KernelPhysicalIdentityMap(LwU64 fbAddress);

// Clone a user-space mapping into another address space (e.g. kernel)
LibosStatus       KernelPagetablePinAndMap(PageTableGlobal * table, LwU64 virtualAddress, PageTableGlobal * source, LwU64 virtualAddressSource, LwU64 size);

LibosStatus       KernelPagetableClearRange(PageTableGlobal * table, LwU64 virtualAddress, LwU64 size);
void              KernelInitPagetable();
void              KernelSoftmmuFlushAll();
LwBool KernelPagetableValidAddress(LwU64 virtualAddress);
extern PageTableGlobal kernelGlobalPagetable;
