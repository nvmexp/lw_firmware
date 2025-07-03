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
#include "sched/timer.h"
#include "trap.h"
#include "diagnostics.h"
#include "extintr.h"
#include "sched/global_port.h"
#include "lwriscv-2.0/sbi.h"
#include "server/spinlock.h"

KernelSpinlock KernelSchedulerLock;

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
Task * LIBOS_SECTION_DMEM_PINNED(KernelInterruptedTask);

Task * LIBOS_SECTION_IMEM_PINNED KernelSchedulerGetTask()
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    //@future: Technically if this is called outside of the scheduler lock,
    //         we're holding a dangling reference.
    return KernelInterruptedTask;
}

LwBool  LIBOS_SECTION_IMEM_PINNED KernelSchedulerIsTaskTrapped(Task * task)
{
   KernelAssertSpinlockHeld(&KernelSchedulerLock);
   return task->taskState == TaskStateWaitTrapped;
}

LwBool LIBOS_SECTION_IMEM_PINNED KernelSchedulerIsTaskWaiting(Task * task)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    return task->taskState != TaskStateReady;
}

LwBool LIBOS_SECTION_IMEM_PINNED KernelSchedulerIsTaskWaitingWithTimeout(Task * task)
{
   KernelAssertSpinlockHeld(&KernelSchedulerLock);
   return task->taskState == TaskStateWaitTimeout;
}

static void KernelSchedulerSelectNextTask() 
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);

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
        contextSwitched = LW_TRUE;
}

void KernelSchedulerRun(Task * new)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    KernelTrace("KERNEL: KernelSchedulerRun(task:%d,...)\n", new->id);
    KernelAssert(KernelSchedulerIsTaskWaiting(new));

    // Cancel any pending timeout
    if (KernelSchedulerIsTaskWaitingWithTimeout(new))
        KernelTimerCancel(new);

    // Clear wait state
    new->taskState = TaskStateReady;

    // KernelInterruptTask can become 0 if we're stuck in the kernel
    // WFI loop.  
    if (!KernelInterruptedTask) 
    {
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
    KernelAssertSpinlockHeld(&KernelSchedulerLock);

    Task * task = KernelInterruptedTask;

    KernelTrace("KERNEL: KernelSchedulerWait(task:%d,...)\n", KernelSchedulerGetTask()->id);

    KernelAssert(task->taskState == TaskStateReady);

    // Transition to wait state
    task->taskState = TaskStateWait;
    
    // Register any timeout
    if (timeoutWakeup)
    {
        task->taskState = TaskStateWaitTimeout;
        KernelSchedulerGetTask()->timestamp = timeoutWakeup;
        KernelTimerEnque(KernelSchedulerGetTask());      
    }
}

LIBOS_SECTION_IMEM_PINNED void KernelSchedulerExit(void)
{ 
    KernelAssertSpinlockHeld(&KernelSchedulerLock);

    KernelTrace("KERNEL: Scheduler return ");
    
    if (KernelSchedulerGetTask())
    {    
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
            if (criticalSection) {
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
    }
    
    if (!KernelInterruptedTask)
    {
        // KernelTrace(" [ WFI ] ");
        KernelTrace(" [ Switching to the next libos partition ] ");
    }

    while (!KernelInterruptedTask) 
    {
        // Wait for timer or external interrupt
#if !LIBOS_TINY
        // __asm__ __volatile__("wfi" ::: );

        // Switch to the next libos partition
        LwU64 initMask = KernelGlobalPage()->initializedPartitionMask;
        for (LwU8 offset = 1; offset <= 63; offset++)
        {
            LwU8 id = (selfPartitionId + offset) % 64;
            if ((initMask & (1 << id)) != 0)
            {
                SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_LIBOS_RUN, 0), 0, 0, 0, 0, id);
            }
        }
#else
        __asm__ __volatile__("wfi" ::: );
#endif        

        // Have any elapsed timers waken up any tasks?
        KernelTimerProcessElapsed(LibosTimeNs());

        // Is an interrupt pending *and* enabled?
        if (REF_VAL64(LW_RISCV_CSR_XIP_XEIP, KernelCsrRead(LW_RISCV_CSR_XIP)) &&
            REF_VAL64(LW_RISCV_CSR_XIE_XEIE, KernelCsrRead(LW_RISCV_CSR_XIE))) 
        {
            KernelExternalInterruptReport();
        }
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
        if (!criticalSection)
            KernelTimerArmInterrupt(nextQuanta);
        else
            KernelTimerArmInterrupt(-1ULL);
    }

    if (contextSwitched)
    {
        KernelTrace(" [MPU programmed] ");
#if LIBOS_TINY
        // Switching between isolation pasid_list? Clear the TLB.
        // @note: This entire sequence belongs in paging/no-paging.c
        // @todo: We need to bind the new address space!!!
        // Switch MPU to next task
        KernelCsrWrite(LW_RISCV_CSR_SMPUIDX2, 0);
        KernelCsrWrite(LW_RISCV_CSR_SMPUVLD,  KernelInterruptedTask->mpuEnables[0]);
        KernelCsrWrite(LW_RISCV_CSR_SMPUIDX2, 1);
        KernelCsrWrite(LW_RISCV_CSR_SMPUVLD,  KernelInterruptedTask->mpuEnables[1]);
#else
        KernelSoftmmuFlushAll();
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
__attribute__((used)) LIBOS_SECTION_IMEM_PINNED void KernelSchedulerPreempt(void) 
{   
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    KernelTrace("KERNEL: KernelSchedulerPreempt()\n");
    preempted = LW_TRUE;
}

/**
 * @brief Handle timer interrupt
 */
LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void KernelSchedulerInterrupt(void)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
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

LIBOS_SECTION_TEXT_COLD void KernelInitScheduler()
{
#if !LIBOS_TINY
    // initialized in libos-data.ld for non-MM builds
    ListConstruct(&KernelSchedulerRunnable);
#endif
}

LIBOS_SECTION_TEXT_COLD void KernelSchedulerLoad()
{
    contextSwitched = LW_TRUE;
    preempted = LW_TRUE;
    timerElapsed = LW_FALSE;
}

LIBOS_SECTION_TEXT_COLD void KernelSchedulerDirtyPagetables(void)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    contextSwitched = LW_TRUE;
}


LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void KernelSyscallTaskCriticalSectionEnter()
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    criticalSection = LW_TRUE;

    // Forces timer reprogram    
    timerElapsed = LW_TRUE;

    KernelTaskReturn();
}

LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void KernelSyscallTaskCriticalSectionLeave()
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    criticalSection = LW_FALSE;

    // Causes the quanta to be programmed again
    timerElapsed = LW_TRUE;

    // Call the timer interrupt routine to check the quanta
    // and return to the next scheduled task.
    KernelTaskReturn();
}
