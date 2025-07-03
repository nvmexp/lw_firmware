/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "libos.h"

LibosStatus LibosPartitionCreate(LwU8 partitionId, LibosPartitionHandle *partition)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_PARTITION_CREATE;
    register LwU64 a0 __asm__("a0") = partitionId;

    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "+r"(a0), "+r"(t0) :: "memory");
    if (((LibosStatus) a0) != LibosOk)
    {
        return (LibosStatus) a0;
    }

    *partition = t0;
    return LibosOk;
}

LibosStatus LibosPartitionMemoryGrantPhysical(LibosPartitionHandle partition, LwU64 base, LwU64 size)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_PARTITION_MEMORY_GRANT_PHYSICAL;
    register LwU64 a0 __asm__("a0") = partition;
    register LwU64 a1 __asm__("a1") = base;
    register LwU64 a2 __asm__("a2") = size;

    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "+r"(a0) : "r"(t0), "r"(a1), "r"(a2) : "memory");
    return (LibosStatus) a0;
}

LibosStatus LibosPartitionMemoryAllocate(LibosPartitionHandle partition, LwU64 *base, LwU64 size, LwU64 alignment)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_PARTITION_MEMORY_ALLOCATE;
    register LwU64 a0 __asm__("a0") = partition;
    register LwU64 a1 __asm__("a1") = size;
    register LwU64 a2 __asm__("a2") = alignment;

    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "+r"(a0), "+r"(t0) : "r"(a1), "r"(a2) : "memory");
    if (((LibosStatus) a0) != LibosOk)
    {
        return (LibosStatus) a0;
    }

    *base = t0;
    return LibosOk;
}

LibosStatus LibosPartitionSpawn(LibosPartitionHandle partition, LwU64 memoryStart, LwU64 memorySize)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_PARTITION_SPAWN;
    register LwU64 a0 __asm__("a0") = partition;
    register LwU64 a1 __asm__("a1") = memoryStart;
    register LwU64 a2 __asm__("a2") = memorySize;

    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "+r"(a0) : "r"(t0), "r"(a1), "r"(a2) : "memory");
    return (LibosStatus) a0;
}
