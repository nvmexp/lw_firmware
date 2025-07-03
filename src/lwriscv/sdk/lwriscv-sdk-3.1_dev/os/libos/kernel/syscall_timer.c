/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "timer.h"
#include "../include/libos.h"
#include "kernel.h"
#include "libos_syscalls.h"
#include "lwtypes.h"
#include "port.h"
#include "libriscv.h"
#include "task.h"
#include "lwriscv-2.0/sbi.h"
#include "scheduler.h"
#include "diagnostics.h"

#if LIBOS_FEATURE_TIMERS
Timer * TimerFindOrRaiseErrorToTask(LibosPortId id, LwU64 grant)
{
    KernelAssert((grant & TimerGrantAll) == grant);
    return (Timer *)ObjectFindOrRaiseErrorToTask(id, grant);
}

__attribute__((used)) LibosStatus LIBOS_SECTION_IMEM_PINNED TimerSyscallTimerSet(LibosPortId timerId, LwU64 timestamp)
{
    Timer * timer = TimerFindOrRaiseErrorToTask(timerId, TimerGrantSet);
    KernelTimerSet(timer, timestamp);
    return LibosOk;
}
#endif