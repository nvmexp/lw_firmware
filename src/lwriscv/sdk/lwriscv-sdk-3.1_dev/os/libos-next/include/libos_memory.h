/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#pragma once
#if !LIBOS_TINY

#include <lwtypes.h>

typedef LwU32 LibosMemoryHandle;

typedef LwU64 LibosMemoryAccess;
// @note: DO NOT change these values without updating softmmu.s
//        It contains tables indexed by this combined mask.
#define LibosMemoryReadable         0x4
#define LibosMemoryWriteable        0x8
#define LibosMemoryExelwtable       0x10
#define LibosMemoryKernelPrivate    0x20 /* internal use */
#define LibosMemoryUncached         0x40

typedef LwU64 LibosMemoryMapFlags;
#define LibosMemoryFlagUnmap      1
#define LibosMemoryFlagPopulate   2
//#define LibosMemoryFlagShare      4

/*
 * @brief Registers a region of physical memory for use by the kernel
 * @note Until the kernel MMU fault handling and kernel blockable threads are
 *       in place this must be mapped through KernelSyscallBootstrapMmap.
 *       Once these features are complete the kernel will create it's own
 *       mapping.
 * @todo NUMA distance map should be programmable here.
 */
#if !LIBOS_TINY
LibosStatus LibosMemoryPoolCreate(LwU64 physicalBase, LwU64 physicalSize, LibosMemoryPoolHandle * nodeHandle);
LibosStatus LibosMemorySetCreate(LibosMemorySetHandle * setHandle);
LibosStatus LibosMemorySetInsert(LibosMemorySetHandle setHandle, LibosMemoryPoolHandle nodeHandle, LwU64 distance);
#endif

#define LibosCommandMemoryAddressSpaceCreate      0x10
#define LibosCommandMemoryAddressSpaceMapPhysical 0x11

typedef struct
{   
    // Address space handle in here on return
    LIBOS_MESSAGE_HEADER(handles, handleCount, 1);

    // Outputs
    LibosStatus             status;
} LibosMemoryAddressSpaceCreate;

LibosStatus LibosAddressSpaceCreate(
    LibosAddressSpaceHandle * handle
);

typedef struct
{   
    // @in: AddressSpace 
    LIBOS_MESSAGE_HEADER(handles, handleCount, 1);

    // Inputs
    LwU64 virtualAddress;
    LwU64 physicalAddress;
    LwU64 size;
    LwU64 flags;

    // Outputs
    LibosStatus             status;
} LibosMemoryAddressSpaceMapPhysical;

LibosStatus LibosAddressSpaceMapPhysical(LibosAddressSpaceHandle addressSpace, LwU64 virtualAddrss, LwU64 physicalAddress, LwU64 size, LwU64 flags, LibosRegionHandle * handle);


/**
 *  @brief Reserves a contiguous region of address space.
 *  
 *  This call strictly reserves the address range and does not
 *  allocate any backing memory or pagetables.
 * 
 *  You may call LibosMemoryMap to bind pages to this region.
 *  @param addressSpace The address space to allocate from. 
 *      (e.g. LibosDefaultAddressSpace)
 *  @param virtualAddress The desired virtual address.
 *      If you pass 0, the operating system will select an appropriate address
 *      and return the value here.
 *  @param size The desired allocation size.  This will be rounded up to the 
 *      appropriate page size.
 *  @param region The region allocation.  
 * 
 * 
 * @example Reserving 4kb of address space, then allocating and mapping memory to it.
 *      LwU64 virtualAddress = 0;
 *      status = LibosMemoryReserve(LibosDefaultAddressSpace, &virtualAddress, 4096, &region);
 *      status = LibosMemoryMap(region, 0, 4096, LibosMemoryRead, LibosMemoryFlagPopulate, 0, 0);
 */
LibosStatus LibosMemoryReserve(
    LibosAddressSpaceHandle addressSpace,
    LwU64                *  virtualAddress,
    LwU64                   size,
    LibosMemoryHandle     * newRegion
);

/** 
 * @brief Maps memory into an already reserved region of the address space.
 * 
 * @param region Region of address space reserved with LibosMemoryReserve.
 * @param offset Offset into the reserved address space to be mapped.
 * @param size   The size of the mapping.  Must be a multiple of the page size.
 * @param access Requested access permissions for the mapping.
 *       such as LibosMemoryReadable | LibosMemoryExelwtable
 * @param flags Requested mapping operation
 *       LibosMemoryFlagPopulate - Allocates memory and maps it within the target region
 * @param memorySet Provides a source for the physical memory pages.
 *                  Pass LibosDefaultMemorySet to use the page pool provided by the spawning task.
 * @example Reserving 4kb of address space, then allocating and mapping memory to it.
 *      LwU64 virtualAddress = 0;
 *      status = LibosMemoryReserve(LibosDefaultAddressSpace, &virtualAddress, 4096, &region);
 *      status = LibosMemoryMap(region, 0, 4096, LibosMemoryRead, LibosMemoryFlagPopulate, 0, 0);
 */
LibosStatus LibosMemoryMap(
    LibosMemoryHandle       region,
    LwU64                   offset,
    LwU64                   size,
    LibosMemoryAccess       access,
    LwU64                   flags,
    LibosMemorySetHandle memorySet
);

/** 
 * @brief Unmaps memory within a region of reserved address sapce.
 * 
 * @param region The region to unmap pages from.
 * @param offset The offset from the start of the region we wish to unmap
 * @param size The length of the unmapping operation
 * 
 */
LibosStatus LibosMemoryUnmap(
    LibosMemoryHandle       region,
    LwU64                   offset,
    LwU64                   size    
);

#endif

#ifndef LIBOS_HOST
void * memcpy(void *destination, const void *source, LwU64 n);
void * memset(void *dest, int c, LwU64 n);
int    strcmp(const char * a, const char * b);
int    strlen(const char * a);
#endif
