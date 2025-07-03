/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "sched/timer.h"
#include "../include/libos.h"
#include "kernel.h"
#include "libos.h"
#include "lwtypes.h"
#include "sched/port.h"
#include "libriscv.h"
#include "task.h"
#include "lwriscv-2.0/sbi.h"
#include "scheduler.h"
#include "diagnostics.h"


__attribute__((used)) LibosStatus LIBOS_SECTION_IMEM_PINNED TimerSyscallTimerSet(LibosPortHandle timerId, LwU64 timestamp)
{
    Timer * timer = KernelObjectResolve(KernelSchedulerGetTask(), timerId, Timer, LibosGrantTimerSet);
    if (!timer)
        return LibosErrorAccess;

    KernelTimerSet(timer, timestamp);
    return LibosOk;
}