/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "kernel.h"
#include "task.h"
#include "scheduler.h"
#include "scheduler-tables.h"
#include "timer.h"
#include "trap.h"
#include "diagnostics.h"
#include "extintr.h"

/**
 * @brief Scheduler KernelSchedulerRunnable queue
 *   * Lwrrently active task is not considered 'KernelSchedulerRunnable'
 */
extern   ListHeader   KernelSchedulerRunnable;

// @todo Merge into single item
static LwBool LIBOS_SECTION_DMEM_PINNED(contextSwitched) = LW_TRUE;
static LwBool LIBOS_SECTION_DMEM_PINNED(preempted)       = LW_TRUE;
static LwBool LIBOS_SECTION_DMEM_PINNED(timerElapsed)    = LW_TRUE;

static LwU64 LIBOS_SECTION_DMEM_PINNED(nextQuanta) = 0;


// @note: Outside of SchedulerExit this contains the last run task.
//        The state of the task may be running or waiting.
//        If the task is Runnable it is not in the KernelSchedulerRunnable
//        queue.
register Task * KernelInterruptedTask asm("tp");

Task * LIBOS_SECTION_IMEM_PINNED KernelSchedulerGetTask()
{
    return KernelInterruptedTask;
}

LwBool  LIBOS_SECTION_IMEM_PINNED KernelSchedulerIsTaskTrapped(Task * task)
{
   return task->taskState == TaskStateWaitTrapped;
}

LwBool LIBOS_SECTION_IMEM_PINNED KernelSchedulerIsTaskWaiting(Task * task)
{
    return task->taskState != TaskStateReady;
}

LwBool LIBOS_SECTION_IMEM_PINNED KernelSchedulerIsTaskWaitingWithTimeout(Task * task)
{
   return task->taskState == TaskStateWaitTimeout;
}

static void KernelSchedulerSelectNextTask() 
{
#if LIBOS_FEATURE_PAGING
    Task * oldTask = KernelInterruptedTask;
#endif
    if (LIBOS_PTR32_EXPAND(ListHeader, KernelSchedulerRunnable.next) == &KernelSchedulerRunnable)
        KernelInterruptedTask = 0;
    else {
        KernelInterruptedTask = LIST_POP_FRONT(KernelSchedulerRunnable, Task, schedulerHeader);
        KernelAssert(KernelInterruptedTask->taskState == TaskStateReady);
    }

#if LIBOS_FEATURE_PAGING
    if (!oldTask || oldTask->pasid != KernelInterruptedTask->pasid)
#endif
        // coverity[uncle]
        contextSwitched = LW_TRUE;
}

void KernelSchedulerRun(Task * new)
{
    KernelTrace("KERNEL: KernelSchedulerRun(task:%d,...)\n", new->id);
    KernelAssert(KernelSchedulerIsTaskWaiting(new));

    // Cancel any pending timeout
    if (KernelSchedulerIsTaskWaitingWithTimeout(new))
        KernelTimerCancel(new);

    // Clear wait state
    new->taskState = TaskStateReady;

    // No task lwrrently running? Switch to it
    if (!KernelInterruptedTask) {
        KernelInterruptedTask = new;
        return;
    }

    if (new == KernelInterruptedTask)
        return;
#if LIBOS_FEATURE_PRIORITY_SCHEDULER==0
    ListPushBack(&KernelSchedulerRunnable, &new->schedulerHeader);
#else
    LIST_INSERT_SORTED(&KernelSchedulerRunnable, &new->schedulerHeader, Task, schedulerHeader, priority);
#endif
}

// @note requires taskState == TaskStateWait
LIBOS_SECTION_IMEM_PINNED void KernelSchedulerWait(LwU64 timeoutWakeup) 
{  
    Task * task = KernelInterruptedTask;

    KernelTrace("KERNEL: KernelSchedulerWait(task:%d,...)\n", KernelSchedulerGetTask()->id);

    KernelAssert(task->taskState == TaskStateReady);

    // Transition to wait state
    task->taskState = TaskStateWait;

    // Register any timeout
    if (timeoutWakeup)
    {
        task->taskState = TaskStateWaitTimeout;
        task->timestamp = timeoutWakeup;
        KernelTimerEnque(task);      
    }
}

LIBOS_SECTION_IMEM_PINNED void KernelSchedulerExit(void)
{ 
    KernelTrace("KERNEL: Scheduler return ");

    KernelAssert(KernelInterruptedTask);
    
    // If the current task is in the wait state, pop another
    if (KernelSchedulerIsTaskWaiting(KernelSchedulerGetTask()))
    {
        KernelSchedulerSelectNextTask();
        KernelTrace(" [ Dequed Task %d ] ", KernelSchedulerGetTask() ? KernelSchedulerGetTask()->id : -1);
    }
    else
    {
        // Is there a more urgent waiting task than our caller?
#if LIBOS_FEATURE_PRIORITY_SCHEDULER
        if (LIBOS_PTR32_EXPAND(ListHeader, KernelSchedulerRunnable.next) != &KernelSchedulerRunnable &&
            CONTAINER_OF(KernelSchedulerRunnable.next, Task, schedulerHeader)->priority < KernelInterruptedTask->priority)
        {
            KernelTrace(" [ Priority Pre-empt ] ");
            preempted = 1;
        }
#endif

        // Don't pre-empt if in a critical section
        if (criticalSection != LibosCriticalSectionNone) {
            KernelTrace(" [ Critical Section ] ");
            preempted = LW_FALSE;
        }

        // Was there a scheduled pre-emption?
        if (preempted)
        {
            // PROBLEM: What happens if there was ALSO another reason
            //          for a task switch? For instance this task might now be in the wait state.

        #if LIBOS_FEATURE_PRIORITY_SCHEDULER==0    
            ListPushBack(&KernelSchedulerRunnable, &KernelInterruptedTask->schedulerHeader);
        #else    
            // Insert the running task into the queue
            LIST_INSERT_SORTED(&KernelSchedulerRunnable, &KernelInterruptedTask->schedulerHeader, Task, schedulerHeader, priority);
        #endif

            KernelSchedulerSelectNextTask();
            KernelTrace(" [ Dequed Task %d ] ", KernelSchedulerGetTask() ? KernelSchedulerGetTask()->id : -1);
        }
    }
    
    // At this point KernelInterruptedTask now contains the task to return to.
    // It is null if no candidate task exists.
    KernelAssert(!KernelInterruptedTask || KernelInterruptedTask->taskState == TaskStateReady);

    if (!KernelInterruptedTask)
        KernelTrace(" [ WFI ] ");

    while (!KernelInterruptedTask) 
    {
        // Wait for timer or external interrupt
        __asm__ __volatile__("wfi" ::: );

        // Have any elapsed timers waken up any tasks?
        KernelTimerProcessElapsed(LibosTimeNs());

#if LIBOS_CONFIG_EXTERNAL_INTERRUPT
        // Is an interrupt pending *and* enabled?
        if (REF_VAL64(LW_RISCV_CSR_XIP_XEIP, KernelCsrRead(LW_RISCV_CSR_XIP)) &&
            REF_VAL64(LW_RISCV_CSR_XIE_XEIE, KernelCsrRead(LW_RISCV_CSR_XIE))) 
        {
            KernelExternalInterruptReport();
        }
#endif    
    }

    KernelAssert(KernelInterruptedTask && KernelInterruptedTask->taskState == TaskStateReady);

    if (contextSwitched || preempted)
    {
        KernelTrace(" [ Quanta Reset ] ");
        // Compute next timeslice
        nextQuanta = LibosTimeNs() + LIBOS_TIMESLICE;        
    }

    if (timerElapsed || contextSwitched || preempted)
    {
        KernelTrace(" [Re-arm] ");
        if (criticalSection == LibosCriticalSectionNone)
            KernelTimerArmInterrupt(nextQuanta);
        else
            KernelTimerArmInterrupt(-1ULL);
    }

    if (contextSwitched)
    {
        KernelTrace(" [MMU] ");
        // Switching between isolation pasid_list? Clear the TLB.
        // @note: This entire sequence belongs in paging/no-paging.c
#if LIBOS_FEATURE_PAGING
        kernel_paging_clear_mpu();
#else
        // Switch MPU to next task
        KernelCsrWrite(LW_RISCV_CSR_SMPUIDX2, 0);
        KernelCsrWrite(LW_RISCV_CSR_SMPUVLD,  KernelInterruptedTask->mpuEnables[0]);
        KernelCsrWrite(LW_RISCV_CSR_SMPUIDX2, 1);
        KernelCsrWrite(LW_RISCV_CSR_SMPUVLD,  KernelInterruptedTask->mpuEnables[1]);
#endif    
    }

    KernelTrace("\n");

    contextSwitched = timerElapsed = preempted = LW_FALSE;
}

/**
 * @brief Prepare for a context switch to the next KernelSchedulerRunnable task
 *   1. Enque the current task to the end of the queue
 *   2. Deque the next task from the head of the queue
 *
 *  @note Intended for context switch on timeslice expiration
 */
LIBOS_SECTION_IMEM_PINNED void KernelSchedulerPreempt(void) 
{   
    KernelTrace("KERNEL: KernelSchedulerPreempt()\n");
    preempted = LW_TRUE;
}

/**
 * @brief Handle timer interrupt
 */
LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void KernelSchedulerInterrupt(void)
{
    KernelTrace("KERNEL: KernelSchedulerInterrupt()\n");
    LwU64 mtime = LibosTimeNs();
    KernelTimerProcessElapsed(mtime);

    timerElapsed = LW_TRUE;

    // Check for current task yield
    if (mtime >= nextQuanta)
    {
        KernelSchedulerPreempt();
    }

    KernelTaskReturn();
}

LIBOS_SECTION_TEXT_COLD void KernelSchedulerLoad(Task * newRunningTask)
{
    KernelTrace("KERNEL: KernelSchedulerLoad(newRunningTask:%d)\n", newRunningTask->id);
    contextSwitched = LW_TRUE;
    preempted = LW_TRUE;
    timerElapsed = LW_FALSE;
    KernelInterruptedTask = newRunningTask;
}

LIBOS_SECTION_TEXT_COLD void KernelSchedulerDirtyPagetables(void)
{
    contextSwitched = LW_TRUE;
}


#if LIBOS_FEATURE_USERSPACE_TRAPS
LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void KernelSyscallTaskCriticalSectionEnter(LibosCriticalSectionBehavior behavior)
{
    criticalSection = behavior;
    KernelTaskReturn();
}
#else
LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void KernelSyscallTaskCriticalSectionEnter()
{
    criticalSection = LibosCriticalSectionPanicOnTrap;

    // Forces timer reprogram    
    timerElapsed = LW_TRUE;

    KernelTaskReturn();
}
#endif

LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void KernelSyscallTaskCriticalSectionLeave()
{
    criticalSection = LibosCriticalSectionNone;

    // Causes the quanta to be programmed again
    timerElapsed = LW_TRUE;

    // Call the timer interrupt routine to check the quanta
    // and return to the next scheduled task.
    KernelTaskReturn();
}
