/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "timer.h"
#include "../include/libos.h"
#include "dev_gsp_riscv_csr_64.h"
#include "dev_riscv_csr_64_addendum.h"
#include "kernel.h"
#include "libos_defines.h"
#include "libos_syscalls.h"
#include "lwtypes.h"
#include "port.h"
#include "riscv-isa.h"
#include "task.h"

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
#define QUANTA_NSEC 100000000ULL

static list_header_t LIBOS_SECTION_DMEM_PINNED(timeouts_pending);
LwU64 LIBOS_SECTION_DMEM_PINNED(nextQuanta) = 0;

/**
 * @brief Initializes the state for timer subsystem
 * * Does not program the hardware
 * * Not called on resume from standby
 *
 * @param void
 */
LIBOS_SECTION_TEXT_COLD void kernel_timer_init(void)
{
    timeouts_pending.next = LIBOS_PTR32_NARROW(&timeouts_pending);
    timeouts_pending.prev = LIBOS_PTR32_NARROW(&timeouts_pending);
}

/**
 * @brief Programs the timer registers
 * * Does not reset the timer state (important for resume from standby)
 *
 * @param void
 *
 */
LIBOS_SECTION_TEXT_COLD void kernel_timer_load(void)
{
    csr_set(LW_RISCV_CSR_XIE, REF_NUM64(LW_RISCV_CSR_XIE_XTIE, 1));
    kernel_timer_reset_quanta();
}

/**
 * @brief Queues a task for a timeout callback
 *  * Assumes the task will be blocking on a port
 *  * Can be canceled through kernel_timeout_cancel
 *  * Assumes target->timestamp initialized with the wakeup time
 *
 * @param[in] target wakeup when mtime >= target->timestamp
 *
 */
LIBOS_SECTION_IMEM_PINNED void kernel_timeout_queue(libos_task_t *target)
{
    kernel_timeout_cancel(target);
    LwU64 timestamp              = target->timestamp;
    list_header_t *insert_before = LIBOS_PTR32_EXPAND(list_header_t, timeouts_pending.next);

    // Find the insertion point (sorted by timestamp)
    while (insert_before != &timeouts_pending &&
           timestamp >= container_of(insert_before, libos_task_t, task_header_timer)->timestamp)
    {
        insert_before = LIBOS_PTR32_EXPAND(list_header_t, insert_before->next);
    }

    // Insert the element before insert_before
    target->task_header_timer.next = LIBOS_PTR32_NARROW(insert_before);
    target->task_header_timer.prev = LIBOS_PTR32_NARROW(insert_before->prev);
    list_link(&target->task_header_timer);
    target->timer_pending = LW_TRUE;
}

/**
 * @brief Cancels a pending timeout on task
 *  * Assumes the task timeout is pending
 *
 */
LIBOS_SECTION_IMEM_PINNED void kernel_timeout_cancel(libos_task_t *target)
{
    if (target->timer_pending)
        list_unlink(&target->task_header_timer);
    target->timer_pending = LW_FALSE;
}

static inline void sbi_timer_set(LwU64 mtimecmp)
{
    register LwU64 sbi_extension_id __asm__("a7") = 0;
    register LwU64 sbi_function_id  __asm__("a6") = 0;
    register LwU64 sbi_mtimecmp_or_return_value __asm__("a0") = mtimecmp;

    // Caution! This SBI is non-compliant and clears GP and TP
    //          This works out as TP is guaranteed to be 0 while in the kernel.
    //          We don't lwrrently use GP in the kernel.
    
    __asm__ __volatile__("ecall" 
        : "+r"(sbi_mtimecmp_or_return_value), "+r"(sbi_extension_id), "+r"(sbi_function_id)
        :
        : "t0", "t1", "t2", "t3", "t4", "t5", "t6", "a1", "a2", "a3", "a4", "a5", "ra");

    if (sbi_mtimecmp_or_return_value != 0)
        panic_kernel();
}

/**
 * @brief Program mtimecmp to min(next timeout, nextQuanta)
 *
 */
LIBOS_SECTION_IMEM_PINNED void kernel_timer_program(LwU64 next)
{
    if (timeouts_pending.next != LIBOS_PTR32_NARROW(&timeouts_pending))
    {
        libos_task_t *next_timeout = container_of(timeouts_pending.next, libos_task_t, task_header_timer);
        if (next_timeout->timestamp < next)
            next = next_timeout->timestamp;
    }
#if LIBOS_CONFIG_RISCV_S_MODE
    sbi_timer_set(LIBOS_CONFIG_NS_TO_TIME(next));
#else
    csr_write(LW_RISCV_CSR_MTIMECMP, LIBOS_CONFIG_NS_TO_TIME(next));
#endif
}

/**
 * @brief Reset the thread timeslice and re-program mtimecmp
 * @note This function should only be called when we're context switching.
 */
LIBOS_SECTION_IMEM_PINNED void kernel_timer_reset_quanta(void)
{
    // Compute next timeslice
    nextQuanta = kernel_get_time_ns() + QUANTA_NSEC;
    kernel_timer_program(nextQuanta);
}

/**
 * @brief Handle timer interrupt
 */
LIBOS_SECTION_IMEM_PINNED LwU64 kernel_timer_process(LwU64 mtime)
{
    LwBool wakeup = LW_FALSE;

    list_header_t * i = LIBOS_PTR32_EXPAND(list_header_t, timeouts_pending.next);

    // Check for elapsed timers
    while (i != &timeouts_pending)
    {
        // Find the task
        libos_task_t *task = container_of(i, libos_task_t, task_header_timer);
        i = LIBOS_PTR32_EXPAND(list_header_t, i->next);

        if (task->timestamp <= mtime)
        {
            kernel_timeout_cancel(task);
            kernel_port_timeout(task);
            if (task!=self)
                kernel_schedule_enque(task);
            wakeup = LW_TRUE;
        }
        else
            break;
    }

    return wakeup;
}

/**
 * @brief Handle timer interrupt
 */
LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void kernel_timer_interrupt(void)
{
    LwU64 mtime = kernel_get_time_ns();
    kernel_timer_process(mtime);

    // Check for current task yield
    if (mtime >= nextQuanta)
    {
        kernel_timer_reset_quanta();
        kernel_syscall_task_yield();
    }

    kernel_timer_program(nextQuanta);
    kernel_return_to_task(self);
}

/**
 * @brief Called from user mode to query the current time in nanoseconds.
 */
LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void kernel_syscall_timer_get_ns(void)
{
    self->state.registers[LIBOS_REG_IOCTL_A0] = kernel_get_time_ns();
    kernel_return();
}

