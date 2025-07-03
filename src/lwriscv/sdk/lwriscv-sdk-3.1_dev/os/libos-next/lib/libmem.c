/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwtypes.h>
#include "libos-config.h"
#include "diagnostics.h"
#include "libos.h"

void *memset(void *dest, int c, LwU64 n)
{
    LwU8 *bytes = (LwU8 *)dest;
    LwU8 *tail  = bytes+n;
#if !LIBOS_TINY
    // Guarantee 1 byte set on each end
    if (n >= 1) 
        bytes[0] = bytes[n-1] = c;
    
    // Guarantee 3 bytes set on each end
    if (n >= 3) 
    {
        bytes[1] = bytes[n-2] = c;
        bytes[2] = bytes[n-3] = c;
    }

    // Clamp pointer and size to a multiple of 4
    // This is safe since the bytes we're losing
    // were already written above.
    LwU32 * start = (LwU32 *)((LwU64)(bytes+3) &~ 3);   // round up to next multiple of 4
    LwU32 * end   = (LwU32 *)((LwU64)tail &~ 3);        // round down 

    LwU32 c32 = ((LwU32)-1)/255 * (LwU8)c;

    // Perform remaining clear 4 bytes at a time
    for (LwU32 * i = start; i != end; i++)
        *i = c32;

    return tail;
#else
    for (LwU8 * i = bytes; i != tail; i++)
        *i = c;
    return tail;
#endif
}

/*
    Optimization notes:
        - Memory
*/
__attribute__((used)) void * memcpy(void *destination, const void *source, LwU64 n)
{
    LwU8 * dest = (LwU8 *)destination;
    LwU8 * destEnd = dest + n;
    LwU8 const * src = (LwU8 const *)source;

#if !LIBOS_TINY
    // Align destination pointer.
    if (n >= 8)
    {
        // Align the destination buffer to 8 bytes
        //        
        // @proof: Check on n cannot underflow
        //        At most 7 bytes will be copied before dest is aligned
        //        we know from condition above that n is at least 8.  No
        while ((LwU64)dest & 7)
            *dest++ = *src++;

        // Source buffer also aligned?
        if (((LwU64)src & 7) == 0)
        {
            // Fast quadword copy
            while (dest + 8 < destEnd)
            {
                *(LwU64 *)dest = *(LwU64 *)src;
                dest += 8, src += 8;
            }
        }
        else
        {
            LwU64 fifo_lower = 0, fifo_upper = 0, fifo_n = 0;

            // While source is misaligned, pull bytes into fifo
            while ((LwU64)src & 7) 
            {
                // Shift byte into bottom of fifo (little endian)
                fifo_lower |= ((LwU64)*src) << fifo_n;
                fifo_n += 8;
                src++;
            }

            // fifo_n <= 56 (since 7 bytes of padding is the most we'd need to align src)
            // wrong: code below was big endian
            /* source and dest are now 8 byte aligned */
            while (dest + 8 <= destEnd)
            {
                // Shift quad into top of fifo
                LwU64 in = *(LwU64 *)src;

                // @note: We consume at most 7 padding bytes in the above loop
                //        implying fifo_n<=56
                //        We consume at least 1 padding byte since we know src 
                //        isn't aligned. fifo_n != 0
                // This ensures neither shift exceeds 63 bits.
                fifo_lower |= in << fifo_n;
                fifo_upper = in >> (64-fifo_n);

                // Shift quad out of bottom of fifo 
                *(LwU64 * )dest = fifo_lower;
                fifo_lower = fifo_upper;
                    
                src += 8;
                dest += 8;
            }

            // The last fifo_n/8 bytes bytes from src weren't written to dest
            src -= fifo_n/8;           
        }
    }
#endif

    // Copy remaining bytes.
    while (dest != destEnd)
        *dest++ = *src++;

    return dest;
}

int strcmp(const char * u, const char* v)
{
    // Consume matching prefix
    while(*u && (*u == *v))
        u++, v++;
    
    // Generate sign by comparison of characters
    return *(const LwU8*)u - *(const LwU8*)v;
}

int strlen(const char * u)
{
    const char * start = u;
    // Consume matching prefix
    while(*u)
        u++;

    return u - start;
}

#if !LIBOS_TINY

LibosStatus LibosAddressSpaceCreate(
    LibosAddressSpaceHandle * handle
)
{
    LibosMemoryAddressSpaceCreate message;
    LwU64 returned = 0;
    message.messageId = LibosCommandMemoryAddressSpaceCreate;
    message.handleCount = 0;
    LibosStatus status = LibosPortSyncSendRecv(LibosKernelServer, &message, sizeof(message), 0, &message, sizeof(message), &returned, LibosTimeoutInfinite, LibosPortFlagTransferHandles);
    if (status != LibosOk)
        return status;
    *handle = message.handles[0].handle;
    return message.status;
}

LibosStatus LibosAddressSpaceMapPhysical(LibosAddressSpaceHandle addressSpace, LwU64 virtualAddress, LwU64 physicalAddress, LwU64 size, LwU64 flags, LibosRegionHandle * handle)
{
    LibosMemoryAddressSpaceMapPhysical message;
    LwU64 returned = 0;
    message.messageId = LibosCommandMemoryAddressSpaceMapPhysical;
    
    message.handleCount = 1;
    message.handles[0].handle = addressSpace;
    message.handles[0].grant = LibosGrantAddressSpaceAll;
    
    message.virtualAddress = virtualAddress;
    message.physicalAddress = physicalAddress;
    message.size = size;
    message.flags = flags;

    LibosStatus status = LibosPortSyncSendRecv(LibosKernelServer, &message, sizeof(message), 0, &message, sizeof(message), &returned, LibosTimeoutInfinite, LibosPortFlagTransferHandles);
    if (status != LibosOk)
        return status;

    *handle = message.handles[0].handle;

    return message.status;
}


/**
 *  Use LibosMemoryReserve, LibosMemoryMap, or LibosMemoryUnmap if possible.
 * 
 *  Creates new region (*region is null)
 *      - Creates a new region in the specified address space
 *      - Address space starts at a virtual address of 'offset' with given size
 *      - If address is null, the kernel will pick a suitable virtual address
 *      - Performs a mapping offset on the entire region
 * 
 *  Re-maps existing region (*region is not null)
 *      - address space is optional
 *      - offset/size describes a slice of this region (offset isn't a VA!)
 *      - Performs a mapping on the selecting pages
 * 
 *  Mapping memory 
 *      - Performs the mapping function in flags 
 *          UNMAP         - Releases any mappings in this region
 *          POPULATE      - Allocates physical pages and assigns them to the region.
 *          CLONE         - Duplicates the page table entries from a slice of a donor region
 *                          This can be used to share memory between tasks.  
 *                          Note: Future updates to the donor region have no impact on this mapping.
 *      - Pages will be marked R/W/X according to the access field
 * 
 */ 
static LibosStatus LibosMemoryMapGeneric(
    LibosAddressSpaceHandle      addressSpace, 
    LibosMemoryHandle      *     region,         
    LwU64                  *     offset,
    LwU64                        size,
    LwU64                        access,
    LwU64                        flags,
    LibosMemorySetHandle memorySet)
{
    // IOCTL marker
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_REGION_MAP_GENERIC;

    register LwU64 a0 __asm__("a0") = addressSpace;
    register LwU64 a1 __asm__("a1") = *region;
    register LwU64 a2 __asm__("a2") = *offset;
    register LwU64 a3 __asm__("a3") = size;
    register LwU64 a4 __asm__("a4") = access;
    register LwU64 a5 __asm__("a5") = flags;
    register LwU64 a6 __asm__("a6") = memorySet;

    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION
                         : "+r"(a1), "+r"(a2)
                         : "r"(t0), "r"(a0), "r"(a3), "r"(a4), "r"(a5), "r"(a6)
                         : "memory");
    *region = a1;
    *offset = a2;
    return (LibosStatus)a0;
}

LibosStatus LibosMemoryReserve(
    LibosAddressSpaceHandle addressSpace,
    LwU64                *  virtualAddress,
    LwU64                   size,
    LibosMemoryHandle    *  newRegion
)
{
    return LibosMemoryMapGeneric(
        addressSpace, 
        newRegion,
        virtualAddress,
        size,
        0, 
        0,
        0);
}


LibosStatus LibosMemoryMap(
    LibosMemoryHandle       region,
    LwU64                   offset,
    LwU64                   size,
    LwU64                   access,
    LwU64                   flags,
    LibosMemorySetHandle memorySet
)
{
    return LibosMemoryMapGeneric(
        0, 
        &region,
        &offset,
        size,
        access, 
        flags,
        memorySet);    
}

LibosStatus LibosMemoryUnmap(
    LibosMemoryHandle       region,
    LwU64                   offset,
    LwU64                   size    
)
{
    return LibosMemoryMapGeneric(
        0, 
        &region,
        &offset,
        size,
        0, 
        LibosMemoryFlagUnmap,
        0);    
}

#endif

void LibosCacheIlwalidateData(void *base, LwU64 size)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_DCACHE_ILWALIDATE;
    register void *a0 __asm__("a0") = base;
    register LwU64 a1 __asm__("a1") = size;
    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "+r"(a0) : "r"(t0), "r"(a1) : "memory");
}

void LibosCacheIlwalidateDataAll()
{
    LibosCacheIlwalidateData(0ULL, -1ULL);
}

#if !LIBOS_TINY

LibosStatus LibosMemoryPoolCreate(LwU64 physicalBase, LwU64 physicalSize, LibosMemoryPoolHandle * nodeHandle)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_PHYSICAL_MEMORY_NODE_ALLOCATE;
    register LwU64 a0 __asm__("a0") = physicalBase;
    register LwU64 a1 __asm__("a1") = physicalSize;
    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "+r"(a0), "+r"(t0) : "r"(a1) : "memory");
    *nodeHandle = t0;

    return (LibosStatus)a0;
}

LibosStatus LibosMemorySetCreate(LibosMemorySetHandle * setHandle)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_PHYSICAL_MEMORY_SET_ALLOCATE;
    register LwU64 a0 __asm__("a0");
    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "+r"(t0), "=r"(a0) : : "memory");
    *setHandle = t0;
    return (LibosStatus)a0;
}

LibosStatus LibosMemorySetInsert(LibosMemorySetHandle setHandle, LibosMemoryPoolHandle nodeHandle, LwU64 distance)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_PHYSICAL_MEMORY_SET_INSERT;
    register LwU64 a0 __asm__("a0") = setHandle;
    register LwU64 a1 __asm__("a1") = nodeHandle;
    register LwU64 a2 __asm__("a2") = distance;
    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "+r"(a0) : "r"(t0), "r"(a1), "r"(a2) : "memory");

    return (LibosStatus)a0;
}

LibosStatus LibosElfOpen(const char  * elfName, LibosHandle * handle)
{
    LibosLoaderElfOpen message;
    LwU64 returned = 0;
    LwU64 nameBufferLength = strlen(elfName) + 1;
    message.messageId = LibosCommandLoaderElfOpen;
    message.handleCount = 0;

    // Serialize the filename
    if (nameBufferLength > sizeof(message.filename))
        return LibosErrorFailed;
    memcpy(message.filename, elfName, nameBufferLength);

    LibosStatus status = LibosPortSyncSendRecv(LibosKernelServer, &message, sizeof(message), 0, &message, sizeof(message), &returned, LibosTimeoutInfinite, LibosPortFlagTransferHandles);
    if (status != LibosOk)
        return status;
    *handle = message.handles[0].handle;
    return message.status;
}

LibosStatus LibosElfMap(LibosAddressSpaceHandle asid, LibosElfHandle elf, LwU64 * entry)
{
    LibosLoaderElfMap message;
    LwU64 returned = 0;
    message.messageId = LibosCommandLoaderElfMap;
    message.handleCount = 2;
    message.handles[0].handle = asid;
    message.handles[0].grant  = LibosGrantAddressSpaceAll;
    message.handles[1].handle = elf;
    message.handles[1].grant  = LibosGrantElfAll;
    LibosStatus status = LibosPortSyncSendRecv(LibosKernelServer, &message, sizeof(message), 0, &message, sizeof(message), &returned, LibosTimeoutInfinite, LibosPortFlagTransferHandles);
    if (status != LibosOk)
        return status;
    *entry = message.entryPoint;
    return message.status;
}

#endif