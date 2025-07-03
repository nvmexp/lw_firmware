/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <lwtypes.h>
#include "libos.h"

#if !LIBOS_TINY
LibosStatus LibosLockCreate(LibosLockHandle *lock)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_LOCK_CREATE;
    register LwU64 a0 __asm__("a0");

    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "=r"(a0), "+r"(t0) :  : "memory");
    *lock = t0;
    return (LibosStatus)a0; 
}
#endif

LibosStatus LibosLockAsyncAcquire(LibosLockHandle lock, LibosShuttleHandle shuttle)
{
    return LibosPortAsyncRecv(shuttle, lock, 0, 0, 0);
}

LibosStatus LibosLockSyncAcquire(LibosLockHandle lock, LwU64 timeout)
{
    return LibosPortSyncRecv(lock, 0, 0, 0, timeout, 0);
}

LibosStatus LibosLockRelease(LibosLockHandle lock)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_LOCK_RELEASE;
    register LwU64 a0 __asm__("a0") = lock;

    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "+r"(a0) : "r"(t0) : "memory");
    return (LibosStatus)a0; 
}
