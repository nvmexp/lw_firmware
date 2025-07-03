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

LibosStatus LibosLockAsyncAcquire(LibosLockId lock, LibosShuttleId shuttle)
{
    return LibosPortAsyncRecv(shuttle, lock, 0, 0);
}

LibosStatus LibosLockSyncAcquire(LibosLockId lock, LwU64 timeout)
{
    return LibosPortSyncRecv(lock, 0, 0, 0, timeout);
}

LibosStatus LibosLockRelease(LibosLockId lock)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_LOCK_RELEASE;
    register LwU64 a0 __asm__("a0") = lock;

    __asm__ __volatile__("ecall" : "+r"(a0) : "r"(t0) : "memory");
    return (LibosStatus)a0; 
}
