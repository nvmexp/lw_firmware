/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_TASK_H
#define LIBOS_TASK_H

#include "../include/libos_task_state.h"
#include "kernel.h"
#include "port.h"
// coverity[include_relwrsion]
#include "timer.h"
#include "scheduler-tables.h"

extern Task *resumeTask;
extern LibosCriticalSectionBehavior  criticalSection;

extern Task TaskInit;

LIBOS_NORETURN void KernelTaskPanic(void);

void LIBOS_NORETURN                         KernelSyscallReturn(LibosStatus status);

void                                        KernelTaskInit(void);
LibosStatus                                 ShuttleSyscallReset(LibosShuttleId reset_shuttle_id);
void                                        KernelPortTimeout(Task *owner);
void                                        kernel_task_replay(LwU32 task_index);
void                                        ListLink(ListHeader *id);
void                                        ListUnlink(ListHeader *id);
void LIBOS_NORETURN                         kernel_port_signal();

LIBOS_NORETURN void KernelSyscallPartitionSwitch();
LIBOS_NORETURN void KernelSyscallTaskCriticalSectionEnter();
LIBOS_NORETURN void KernelSyscallTaskCriticalSectionLeave();
void * KernelTaskResolveObject(Task * self, LwU64 id, LwU64 grant);

extern LwU64 userXStatus;

LIBOS_NORETURN LIBOS_SECTION_IMEM_PINNED void KernelTaskReturn();


#endif
