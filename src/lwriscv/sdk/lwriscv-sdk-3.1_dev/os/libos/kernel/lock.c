/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lock.h"
#include "scheduler.h"
#include "task.h"

LibosStatus KernelLockRelease(Lock *lock)
{
    if (lock->holdCount == 0 || lock->holder != KernelSchedulerGetTask()->id)
    {
        return LibosErrorAccess;
    }

    if (--lock->holdCount)
    {
        return LibosOk;
    }

    ShuttleBindSend(&lock->unlockShuttle, 0, 0);
    PortSendRecvWait(&lock->unlockShuttle, &lock->port, 0, 0, TaskStateReady, 0, 0, LibosPortLockOperation);
}


__attribute__((used)) LibosStatus KernelSyscallLockRelease(LibosLockId lockHandle)
{
    Lock *lock = KernelTaskResolveObject(KernelSchedulerGetTask(), lockHandle, LockGrantAll);
    if (!lock)
    {
        return LibosErrorAccess;
    }

    LibosStatus status = KernelLockRelease(lock);
    return status;
}

