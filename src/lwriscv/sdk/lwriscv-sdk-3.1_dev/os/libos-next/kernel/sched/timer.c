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
#include "libos_syscalls.h"
#include "lwtypes.h"
#include "sched/port.h"
#include "libriscv.h"
#include "task.h"
#include "lwriscv-2.0/sbi.h"
#include "scheduler.h"
#include "diagnostics.h"
#include "sched/global_port.h"

/**
 *
 * @file timer.c
 *
 * @brief Management of the hardware timer
 *   * Task scheduling
 *   * Timeout tracking
 */

/**
 * @brief Default thread quanta (timslice)
 *  * Basic scheduler resets quanta on context switch
 *  @todo Transition to ready/running queues and binned priority scheduling
 *
 */

// Pending port timeouts
static ListHeader LIBOS_SECTION_DMEM_PINNED(timeoutsPending);

// Pending kernel timer objects
static ListHeader LIBOS_SECTION_DMEM_PINNED(timersPending);

/**
 * @brief Initializes the state for timer subsystem
 * * Does not program the hardware
 * * Not called on resume from standby
 *
 * @param void
 */
LIBOS_SECTION_TEXT_COLD void KernelInitTimer(void)
{
    timeoutsPending.next = LIBOS_PTR32_NARROW(&timeoutsPending);
    timeoutsPending.prev = LIBOS_PTR32_NARROW(&timeoutsPending);
    timersPending.next = LIBOS_PTR32_NARROW(&timersPending);
    timersPending.prev = LIBOS_PTR32_NARROW(&timersPending);
}

/**
 * @brief Programs the timer registers
 * * Does not reset the timer state (important for resume from standby)
 *
 * @param void
 *
 */
LIBOS_SECTION_TEXT_COLD void KernelTimerLoad(void)
{
    KernelCsrSet(LW_RISCV_CSR_XIE, REF_NUM64(LW_RISCV_CSR_XIE_XTIE, 1));
}

/**
 * @brief Queues a task for a timeout callback
 *  * Assumes the task will be blocking on a port
 *  * Can be canceled through KernelTimerCancel
 *  * Assumes target->timestamp initialized with the wakeup time
 *
 * @param[in] target wakeup when mtime >= target->timestamp
 *
 */
LIBOS_SECTION_IMEM_PINNED void KernelTimerEnque(Task *target)
{
    LIST_INSERT_SORTED(&timeoutsPending, &target->timerHeader, Task, timerHeader, timestamp);
}

/**
 * @brief Cancels a pending timeout on task
 *  * Assumes the task timeout is pending
 *
 */
LIBOS_SECTION_IMEM_PINNED void KernelTimerCancel(Task *target)
{
    ListUnlink(&target->timerHeader);
}

/**
 * @brief Program mtimecmp to min(next timeout, nextQuanta)
 *
 */
LIBOS_SECTION_IMEM_PINNED void KernelTimerArmInterrupt(LwU64 nextPreempt)
{
    if (timeoutsPending.next != LIBOS_PTR32_NARROW(&timeoutsPending))
    {
        Task *next_timeout = CONTAINER_OF(timeoutsPending.next, Task, timerHeader);
        if (next_timeout->timestamp < nextPreempt)
            nextPreempt = next_timeout->timestamp;
    }
#if LIBOS_CONFIG_RISCV_S_MODE
    SeparationKernelTimerSet(LIBOS_CONFIG_TIME_NS(nextPreempt));
#else
    KernelCsrWrite(LW_RISCV_CSR_MTIMECMP, LIBOS_CONFIG_TIME_NS(nextPreempt));
#endif
}

/**
 * @brief Handle timer interrupt
 */
LIBOS_SECTION_IMEM_PINNED LwU64 KernelTimerProcessElapsed(LwU64 mtime)
{
    LwBool wakeup = LW_FALSE;

    // Check for elapsed timeouts
    for (ListHeader *header = LIBOS_PTR32_EXPAND(ListHeader, timeoutsPending.next);
        header != &timeoutsPending;)
    {
        Task *task = CONTAINER_OF(header, Task, timerHeader);
        header = LIBOS_PTR32_EXPAND(ListHeader, header->next);

        KernelAssert(KernelSchedulerIsTaskWaitingWithTimeout(task));

        if (task->timestamp > mtime)
            break;

        ListUnlink(&task->timerHeader);
        KernelPortTimeout(task);
        wakeup = LW_TRUE;
    }

    // Check for elapsed kernel timer objects
    for (ListHeader *header = LIBOS_PTR32_EXPAND(ListHeader, timersPending.next);
        header != &timersPending;)
    {
        Timer * timer = CONTAINER_OF(header, Timer, timerHeader);
        header = LIBOS_PTR32_EXPAND(ListHeader, header->next);

        if (timer->timestamp > mtime) 
            break;

        // Clear pending timer
        ListUnlink(&timer->timerHeader);
        timer->pending = LW_FALSE;
    
        // Fire the waiting shuttles on the timer
        while (PortHasReceiver(&timer->port))
        {
            Shuttle *waiter = PortDequeReceiver(&timer->port);

            // Retire the waiting shuttle
            waiter->errorCode   = LibosOk;
            waiter->payloadSize = 0;
            ShuttleRetireSingle(waiter);
        }

        wakeup = LW_TRUE;
    }

    return wakeup;
}

__attribute__((used)) void LIBOS_SECTION_IMEM_PINNED KernelTimerSet(Timer * timer, LwU64 timestamp)
{
    // Clear existing pending timer
    if (timer->pending) 
        ListUnlink(&timer->timerHeader);

    // Are we setting a timer?
    if (timestamp != -1ULL)
    {
        LIST_INSERT_SORTED(&timersPending, &timer->timerHeader, Timer, timerHeader, timestamp);
        timer->pending = LW_TRUE;    
    }
}
