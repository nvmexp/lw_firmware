/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_SCHEDULER_H
#define LIBOS_SCHEDULER_H
#include "kernel.h"
#include "server/spinlock.h"

extern KernelSpinlock KernelSchedulerLock;

void KernelInitScheduler();

/**
 * @brief Returns the TCB of the task that was most recently interrupted.
 *
 * @note: This task might be in the waiting state due to subsequent calls
 *        to KernelSchedulerWait while in the kernel.
 */
Task *      KernelSchedulerGetTask();

/**
 * @brief Returns true iff the task has trapped due to a call to
 *        KernelTaskPanic.
 * 
 *  This will occur due to exelwtion errors such as load/store traps
 *  as well as deferred conditions ECC error containment.
 *
 *  Internally the kernel will issue a port call on behalf of the task
 *  to notify the init task of the crash and wait for a reply on the
 *  reply port.  
 * 
 *  @note The trapped flag is different from a port wait in that
 *        ShuttleWriteSyscallRegisters is skipped to avoid overwriting 
 *        task registers.
 */
LwBool              KernelSchedulerIsTaskTrapped(Task * task);

/**
 * @brief The task has issue a port wait and has a wake-up timeout counting down
 * 
 */
LwBool              KernelSchedulerIsTaskWaitingWithTimeout(Task * task);


/**
 * @brief Is the task in any wait state [e.g. wait, wait with timeout, trapped].
 */
LwBool              KernelSchedulerIsTaskWaiting(Task * task);

/**
 * @brief Transitions the current task to a waiting state.
 *
 * The task will not be scheduled again until KernelSchedulerRun is called
 * 
 * @note: The transition does not take effect until KernelSchedulerExit() is called.
 *  
 */
void KernelSchedulerWait(LwU64 timeoutWakeup);

/**
 * @brief Re-schedules the task
 * 
 * Removes any pending wait state, cancels any outstanding timeouts, and returns the task
 * to the scheduler queue.
 * 
 * Calling KernelSchedulerWait followed by KernelSchedulerRun in the same scheduler invocation
 * does not reset the timeslice.  Transitions only take effect on KernelSchedulerExit.
 */
void KernelSchedulerRun(Task * n);

/**
 * @brief Schedules a pre-emption on the next scheduler exit
 */
void KernelSchedulerPreempt();

/**
 * @brief Forces an MMU ilwalidate on next scheduler exit.
 */
LIBOS_SECTION_TEXT_COLD void KernelSchedulerDirtyPagetables();

/**
 * @brief This is the entry point for the main timer interrupt.
 * 
 *  The scheduler is interrupt based.  The kernel computes the time until the next timer or 
 *  timeslice event.  
 * 
 */
LIBOS_NORETURN void KernelSchedulerInterrupt(void);

/**
 * @brief Initializes any hardware used by the scheduler
 */
LIBOS_SECTION_TEXT_COLD void KernelSchedulerLoad();

/**
 * @brief Called before KernelContextRestore to exit back to the desired task
 * 
 * This call applies all previous state updates from KernelSchedulerRun,
 * KernelSchedulerWait, and KernelSchedulerPreempt
 * and returns to the appropriate task.
 */
LIBOS_SECTION_IMEM_PINNED void KernelSchedulerExit();
#endif