/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "task.h"
#include "../include/libos.h"
#include "dev_gsp_riscv_csr_64.h"
#include "dev_riscv_csr_64_addendum.h"
#include "interrupts.h"
#include "kernel.h"
#include "kernel_copy.h"
#include "libos_defines.h"
#include "libos_status.h"
#include "libos_syscalls.h"
#include "memory.h"
#include "mmu.h"
#include "paging.h"
#include "port.h"
#include "profiler.h"
#include "riscv-isa.h"
#include "timer.h"
#include "lwtypes.h"
#include "extintr.h"
#include "suspend.h"

/**
 *
 * @file task.c
 *
 * @brief Scheduler implementation
 */

// Number of elements in task_by_id array
LIBOS_REFERENCE_LINKER_SYMBOL_AS_INTEGER(task_by_id_count);

/**
 * @brief Task to start at the end of kernel_init.  Used to ensure that a bootstrap after suspend
 * returns to the prior active task.
 *   * kernel_task_init - initializes this to the init-task
 *   * kernel_processor_suspend - sets this to the lwrrently running task at time of suspend
 */
libos_task_t *LIBOS_SECTION_DMEM_PINNED(resume_task) = 0;

/**
 * @brief Scheduler ready queue
 *   * Lwrrently active task is not considered 'ready'
 */
list_header_t LIBOS_SECTION_DMEM_PINNED(ready) = {0, 0};

LwU64 user_xstatus;

/**
 * @brief One time initialization of the task state.
 *
 */
LIBOS_SECTION_TEXT_COLD void kernel_task_init(void)
{
    // Set the resume task if not already set
    ready.next  = LIBOS_PTR32_NARROW(&ready);
    ready.prev  = LIBOS_PTR32_NARROW(&ready);
    resume_task = &task_init_instance;

    // Tasks run with user privileges and enabled
    // @note Debugger init task could be enabled by setting _MPP here to _MACHINE
    user_xstatus = csr_read(LW_RISCV_CSR_XSTATUS);
    user_xstatus = user_xstatus & ~REF_SHIFTMASK64(LW_RISCV_CSR_XSTATUS_XPP);
    user_xstatus |= REF_DEF64(LW_RISCV_CSR_XSTATUS_XPP, _USER);
    user_xstatus |= REF_DEF64(LW_RISCV_CSR_XSTATUS_XPIE, _ENABLE);
}

/**
 * @brief Queues a task onto the end of the ready queue
 *   * Has no effect if called on the idle task
 *   @note The lwrrently running task is not ready.
 */
LIBOS_SECTION_IMEM_PINNED void kernel_schedule_enque(libos_task_t *id)
{
    // Insert at the end of the queue
    id->task_header_scheduler.prev = LIBOS_PTR32_NARROW(&ready);
    id->task_header_scheduler.next = LIBOS_PTR32_NARROW(ready.next);
    list_link(&id->task_header_scheduler);
}

/**
 * @brief Deque the head of the ready queue (next task)
 *   * Returns the idle task if the queue is empty
 */
LIBOS_SECTION_IMEM_PINNED libos_task_t *kernel_schedule_deque(void)
{
    list_header_t *tail = LIBOS_PTR32_EXPAND(list_header_t, ready.prev);
    list_unlink(tail);
    return container_of(tail, libos_task_t, task_header_scheduler);
}


LIBOS_NORETURN LIBOS_SECTION_IMEM_PINNED void kernel_schedule_wait(void)
{
    // Update the timer queue with new wait state
    if (self->task_state & LIBOS_TASK_STATE_TIMER_WAIT) {
        kernel_timeout_queue(self);
    }

    // Idle state
    while (1)
    {
        // Is the current task runnable? Return.
        if (self->task_state == LIBOS_TASK_STATE_NORMAL) {
            kernel_timer_program(nextQuanta);
            kernel_return_to_task(self);
        }

        // Is there a runnable task in the queue?
        if (LIBOS_PTR32_EXPAND(list_header_t, ready.next) != &ready)
            kernel_return_to_task(kernel_schedule_deque());

        // Update the timer programming and remove quanta timer
        kernel_timer_program(-1ULL);  // no next quanta

        // Wait for timer or external interrupt
        __asm__ __volatile__("wfi" ::: );

        // Have any elapsed timers waken up any tasks?
        kernel_timer_process(kernel_get_time_ns());

        // Is an interrupt pending *and* enabled?
        if (REF_VAL64(LW_RISCV_CSR_XIP_XEIP, csr_read(LW_RISCV_CSR_XIP)) &&
            REF_VAL64(LW_RISCV_CSR_XIE_XEIE, csr_read(LW_RISCV_CSR_XIE)))
        {
            kernel_external_interrupt_report();
        }
    }

    kernel_return_to_task(kernel_schedule_deque());
}


/**
 * @brief Prepare for a context switch to the next ready task
 *   1. Enque the current task to the end of the queue
 *   2. Deque the next task from the head of the queue
 *
 *  @note Intended for context switch on timeslice expiration
 */
LIBOS_SECTION_IMEM_PINNED libos_task_t *kernel_reschedule_and_deque(void)
{
    if (LIBOS_PTR32_EXPAND(list_header_t, ready.next) == &ready)
        return self;

    kernel_schedule_enque(self);
    return kernel_schedule_deque();
}

/**
 * @brief Reset the task state and return LIBOS_ERROR_ILWALID_ACCESS from current environment call
 *
 * @note This is used in place of explicit returns for code size reasons (no register restore
 * needed)
 */
LIBOS_SECTION_TEXT_COLD LIBOS_NORETURN void kernel_return_access_errorcode(void)
{
    self->task_state                                 = LIBOS_TASK_STATE_NORMAL;
    self->state.registers[LIBOS_REG_IOCTL_STATUS_A0] = LIBOS_ERROR_ILWALID_ACCESS;
    kernel_return_to_task(self);
}

void kernel_continuation();

/**
 * @brief Queries register state for task (requires debug permission on the target task)
 */
void LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD kernel_syscall_task_query(
    LwU64           task_index, // a0
    LibosTaskState *payload    // a1
)
{
    if (task_index >= task_by_id_count)
        kernel_return_access_errorcode();

    libos_task_t *task = task_by_id[task_index];

    if (!(pasid_debug_mask_by_id[task->pasid] & (1 << self->pasid)))
        kernel_return_access_errorcode();

    validate_memory_access_or_return((LwU64)payload, sizeof(*payload), LW_TRUE /* write*/);

    self->state.registers[LIBOS_REG_IOCTL_A0] = LIBOS_OK;

    self->continuation       = LIBOS_CONTINUATION_NONE;
    self->kernel_write_pasid = self->pasid;
    /* we don't read from user-space mappings */
    kernel_port_copy_and_continue((LwU64)payload, (LwU64)&task->state, sizeof(task->state));
}

void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN kernel_continuation(void)
{
    switch (self->continuation)
    {
    // Used for non-port copies
    case LIBOS_CONTINUATION_NONE:
        kernel_return();
        break;

    case LIBOS_CONTINUATION_RECV:
        kernel_port_continue_recv();
        break;

    case LIBOS_CONTINUATION_WAIT:
        kernel_port_continue_wait();
        break;

    default:
        __builtin_unreachable();
    }
}

LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void kernel_task_panic(void)
{
    extern libos_port_t task_init_panic_port_instance;

    kernel_timeout_cancel(self);

    libos_shuttle_t *crash_shuttle = LIBOS_PTR32_EXPAND(
        libos_shuttle_t, self->resources[LIBOS_SHUTTLE_INTERNAL_TRAP - 1] & LIBOS_RESOURCE_PTR_MASK);
    libos_shuttle_t *replay_shuttle = LIBOS_PTR32_EXPAND(
        libos_shuttle_t, self->resources[LIBOS_SHUTTLE_INTERNAL_REPLAY - 1] & LIBOS_RESOURCE_PTR_MASK);
    libos_port_t *replay_port = LIBOS_PTR32_EXPAND(
        libos_port_t, self->resources[LIBOS_SHUTTLE_INTERNAL_REPLAY_PORT - 1] & LIBOS_RESOURCE_PTR_MASK);

    kernel_port_send_recv_wait(
        crash_shuttle, &task_init_panic_port_instance, 0, 0,
        replay_shuttle, replay_port, (LwU64)&self->state, sizeof(self->state),
        LIBOS_TASK_STATE_PORT_WAIT | LIBOS_TASK_STATE_TRAPPED, replay_shuttle, -1ULL);
}

LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void kernel_return(void)
{
    kernel_raw_return_to_task(self);
}

LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void kernel_return_to_task(libos_task_t *new_self)
{
    // Context switching? Reset the quanta for timeslicing.
    if (new_self != self)
    {
        kernel_timer_reset_quanta();
    }

    // Switching between isolation pasid_list? Clear the TLB.
#ifdef LIBOS_FEATURE_ISOLATION
    if (self->pasid != new_self->pasid)
    {
        kernel_mmu_clear_mpu();
    }
#endif

    kernel_raw_return_to_task(new_self);
}

LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void kernel_syscall_task_yield(void)
{
    kernel_return_to_task(kernel_reschedule_and_deque());
}

LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void kernel_syscall_processor_suspend(void)
{
    kernel_prepare_for_suspend();

    while (LW_TRUE)
    {
    }
}

LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void kernel_syscall_task_memory_read(
    LwU64 localAddress, task_index_t task_index, LwU64 remoteAddress, LwU64 length)
{
    if (task_index >= task_by_id_count)
        kernel_return_access_errorcode();

    libos_task_t *remoteTask = task_by_id[task_index];

    // Validate we are the designated debugger for the remote task
    if (!(pasid_debug_mask_by_id[remoteTask->pasid] & (1 << self->pasid)))
        kernel_return_access_errorcode();

    // Validate the current task has write access to the target buffer
    validate_memory_access_or_return((LwU64)localAddress, length, LW_TRUE /* write*/);

    // Validate that the remote task has read access to the remote buffer
    validate_memory_pasid_access_or_return(
        (LwU64)remoteAddress, length, LW_FALSE /* read */, remoteTask->pasid);

    // Once the copy is complete, return to userspace
    self->continuation                        = LIBOS_CONTINUATION_NONE;
    self->state.registers[LIBOS_REG_IOCTL_A0] = LIBOS_OK;

    // Bind the addresses spaces for the copy
    self->kernel_write_pasid = self->pasid;
    self->kernel_read_pasid  = remoteTask->pasid;

    kernel_port_copy_and_continue((LwU64)localAddress, remoteAddress, length);
}

LIBOS_SECTION_TEXT_COLD libos_task_t *kernel_task_resolve_or_return_errorcode(task_index_t remote_task)
{
    if (remote_task == LIBOS_TASK_SELF)
        return self;

    if (remote_task < task_by_id_count)
    {
        // @bug Do we have debug access?
        return task_by_id[remote_task];
    }

    kernel_return_access_errorcode();
}
