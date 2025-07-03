/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "sched/lock.h"
#include "scheduler.h"
#include "task.h"

LibosStatus KernelLockRelease(Lock *lock)
{
#if !LIBOS_TINY
    if (lock->holdCount == 0 || lock->holder != KernelSchedulerGetTask())
#else
    if (lock->holdCount == 0 || lock->holder != KernelSchedulerGetTask()->id)
#endif
    {
        return LibosErrorAccess;
    }

    if (--lock->holdCount)
    {
        return LibosOk;
    }

    ShuttleBindSend(&lock->unlockShuttle, 0, 0, 0);
    return PortSendRecvWait(&lock->unlockShuttle, &lock->port, 0, 0, TaskStateReady, 0, 0, 0);
}

__attribute__((used)) LibosStatus KernelSyscallLockRelease(LibosLockHandle lockHandle)
{
    Lock *lock = KernelObjectResolve(KernelSchedulerGetTask(), lockHandle, Lock, LibosGrantLockAll);
    if (!lock)
    {
        return LibosErrorAccess;
    }

    LibosStatus status = KernelLockRelease(lock);
    KernelPoolRelease(lock);
    return status;
}

void KernelLockDestroy(Lock *lock)
{
    KernelSyscallShuttleReset(&lock->unlockShuttle);
    KernelPortDestroy(&lock->port);
}
