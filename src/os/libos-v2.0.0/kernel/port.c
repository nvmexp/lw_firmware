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
#include "paging.h"
#include "port.h"
#include "riscv-isa.h"
#include "timer.h"
#include <sdk/lwpu/inc/lwtypes.h>

static void LIBOS_NORETURN kernel_port_continue_send(libos_shuttle_t *send_shuttle, libos_port_t *send_port);

/**
 *
 * @file port.c
 *
 * @brief Asynchronous Inter-process Communication
 */

void kernel_continuation();
LIBOS_SECTION_IMEM_PINNED libos_task_t *kernel_schedule_deque(void);

/**
 * @brief Lookups and validates access in the task-local resource table.
 *
 * @note These are used for ports/shuttles.  requires_any_of access bits imply a specific type.
 */
static LIBOS_SECTION_IMEM_PINNED void *
kernel_resource_validate_or_raise_error_to_task(LwU64 resource_id, LwU32 requires_any_of)
{
    LwU64 resource_index = resource_id - 1;

#ifdef LIBOS_FEATURE_ISOLATION
    // Validate this is a port
    if (resource_index >= self->resource_count)
    {
        kernel_return_access_errorcode();
    }

    // Ensure the resource is valid, in bounds, and granted required permissions
    if ((self->resources[resource_index] & requires_any_of) == 0u)
    {
        kernel_return_access_errorcode();
    }
#endif

    // Perform the lookup
    return (void *)((LwUPtr)(self->resources[resource_index] & LIBOS_RESOURCE_PTR_MASK));
}

static LIBOS_SECTION_IMEM_PINNED void
kernel_transcribe_status(libos_task_t *owner, libos_shuttle_t *completed_shuttle)
{
    if (owner->task_state & LIBOS_TASK_STATE_TRAPPED)
        return;

    owner->state.registers[LIBOS_REG_IOCTL_STATUS_A0]           = completed_shuttle->error_code;
    owner->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_RECV_SIZE] = completed_shuttle->payload_size;

    owner->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_REMOTE_TASK] = completed_shuttle->origin_task;
    owner->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_OUT_SHUTTLE] = completed_shuttle->task_local_port_handle;
}

void LIBOS_SECTION_IMEM_PINNED kernel_port_timeout(libos_task_t *owner)
{
    owner->state.registers[LIBOS_REG_IOCTL_STATUS_A0] = LIBOS_ERROR_TIMEOUT;
    owner->task_state                                 = LIBOS_TASK_STATE_NORMAL;
}

static void LIBOS_SECTION_IMEM_PINNED kernel_unlink_reply_credentials(libos_shuttle_t *reset_shuttle)
{
    if (reset_shuttle->reply_to_shuttle)
        LIBOS_PTR32_EXPAND(libos_shuttle_t, reset_shuttle->reply_to_shuttle)->granted_shuttle = 0;
}

static void LIBOS_SECTION_IMEM_PINNED kernel_unlink_remote_reply_credentials(libos_shuttle_t *reset_shuttle)
{
    if (reset_shuttle->granted_shuttle)
        LIBOS_PTR32_EXPAND(libos_shuttle_t, reset_shuttle->granted_shuttle)->reply_to_shuttle = 0;
}

LIBOS_SECTION_IMEM_PINNED void kernel_shuttle_retire_single(libos_shuttle_t *shuttle)
{
    libos_task_t *owner = task_by_id[shuttle->owner];

    if ((owner->task_state & LIBOS_TASK_STATE_PORT_WAIT) &&
        (LIBOS_PTR32_EXPAND(libos_shuttle_t, owner->sleeping_on_shuttle) == shuttle ||
         !owner->sleeping_on_shuttle))
    {
        shuttle->state = LIBOS_SHUTTLE_STATE_COMPLETE_IDLE;

        kernel_timeout_cancel(owner);
        kernel_transcribe_status(owner, shuttle);

        owner->task_state = LIBOS_TASK_STATE_NORMAL;

        if (owner != self)
            kernel_schedule_enque(owner);
    }
    else
    {
        shuttle->shuttle_completed_link.prev = LIBOS_PTR32_NARROW(&owner->task_completed_shuttles);
        shuttle->shuttle_completed_link.next = LIBOS_PTR32_NARROW(owner->task_completed_shuttles.next);
        list_link(&shuttle->shuttle_completed_link);

        shuttle->state = LIBOS_SHUTTLE_STATE_COMPLETE_PENDING;
    }
}

// @note Either this was an asynchronous operation (and we return to self), or self was one of the
// waiters
LIBOS_SECTION_IMEM_PINNED void kernel_shuttle_retire_pair(libos_shuttle_t *source, libos_shuttle_t *sink)
{
    sink->origin_task = source->owner;

    kernel_unlink_remote_reply_credentials(sink);

    //
    //  Did we just receive a message from a source that's a sendrecv
    //
    libos_shuttle_t *partnered_waiter = LIBOS_PTR32_EXPAND(libos_shuttle_t, source->partner_recv_shuttle);
    if (partnered_waiter && partnered_waiter->state == LIBOS_SHUTTLE_STATE_RECV)
    {
        sink->reply_to_shuttle            = LIBOS_PTR32_NARROW(partnered_waiter);
        partnered_waiter->granted_shuttle = LIBOS_PTR32_NARROW(sink);
    }
    else
        sink->reply_to_shuttle = 0;
    source->reply_to_shuttle = 0;

    kernel_shuttle_retire_single(source);
    kernel_shuttle_retire_single(sink);
}

void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN kernel_syscall_shuttle_reset(LwU64 reset_shuttle_id)
{
    libos_shuttle_t *reset_shuttle    = (libos_shuttle_t *)kernel_resource_validate_or_raise_error_to_task(
        reset_shuttle_id, LIBOS_RESOURCE_SHUTTLE_GRANT);

    /*
     @note No one is waiting on this shuttle as only a single task has privileges for the shuttle
     (us).
    */
    switch (reset_shuttle->state)
    {
    case LIBOS_SHUTTLE_STATE_COMPLETE_PENDING:
        list_unlink(&reset_shuttle->shuttle_completed_link);
        /* fallthrough */

    case LIBOS_SHUTTLE_STATE_COMPLETE_IDLE:
        kernel_unlink_reply_credentials(reset_shuttle);
        break;

    case LIBOS_SHUTTLE_STATE_SEND:
        list_unlink(&reset_shuttle->port_sender_link);
        break;

    case LIBOS_SHUTTLE_STATE_RECV:
        kernel_unlink_remote_reply_credentials(reset_shuttle);
        reset_shuttle->waiting_on_port->waiting_future = 0;
        break;
    }

    // Mark transaction as completed
    reset_shuttle->state = LIBOS_SHUTTLE_STATE_RESET;

    self->state.registers[LIBOS_REG_IOCTL_STATUS_A0] = LIBOS_OK;
    kernel_return();
}


void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN kernel_port_send_recv_wait(
    libos_shuttle_t * send_shuttle, libos_port_t * send_port, LwU64 send_payload, LwU64 send_payload_size, 
    libos_shuttle_t * recv_shuttle, libos_port_t * recv_port, LwU64 recv_payload, LwU64 recv_payload_size,
    LwU64 new_task_state, libos_shuttle_t * wait_shuttle, LwU64 wait_timeout)
{
    /*
     * Validate Send
     */
    if (send_shuttle)
    {
        // Is the send_shuttle_id slot empty?
        if (send_shuttle->state >= LIBOS_SHUTTLE_STATE_SEND)
            kernel_return_access_errorcode();
       
    }
    self->sending_shuttle = LIBOS_PTR32_NARROW(send_shuttle);

    /*
     * Recv Validate
     */
    if (recv_shuttle)
    {
        // Is the shuttle_id slot empty?  Is there already a waiter on this port (we can't queue
        // waiters)
        if (recv_shuttle->state >= LIBOS_SHUTTLE_STATE_SEND || recv_port->waiting_future)
            kernel_return_access_errorcode();           
    }
    self->receiving_shuttle = LIBOS_PTR32_NARROW(recv_shuttle);
    self->receiving_port    = LIBOS_PTR32_NARROW(recv_port);

    /*
     *  Past this we're committed to completing the transaction
     */

    /*
     * Wait buildout
     */
    if (new_task_state != LIBOS_TASK_STATE_NORMAL)
    {
        self->sleeping_on_shuttle = LIBOS_PTR32_NARROW(wait_shuttle);
        self->task_state = new_task_state;
        if (new_task_state & LIBOS_TASK_STATE_TIMER_WAIT)
        {
            self->timestamp  = self->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_TIMEOUT] + kernel_get_time_ns();
        }
    }

    /*
     * Send Shuttle Buildout
     */
    if (send_shuttle)
    {
        // Unlink reply credential before overwriting the data
        kernel_unlink_reply_credentials(send_shuttle);

        send_shuttle->partner_recv_shuttle = LIBOS_PTR32_NARROW(recv_shuttle);

        // Fill the send_shuttle_id slot out
        send_shuttle->state           = LIBOS_SHUTTLE_STATE_SEND;
        send_shuttle->payload_address = send_payload;
        send_shuttle->payload_size    = send_payload_size;
        send_shuttle->error_code      = LIBOS_OK;
    }

    /*
     * Recv Shuttle Buildout
     */
    if (recv_shuttle)
    {
        // Fill the shuttle_id slot out
        recv_shuttle->state           = LIBOS_SHUTTLE_STATE_RECV;
        recv_shuttle->granted_shuttle = 0;
        recv_shuttle->payload_address = recv_payload;
        recv_shuttle->payload_size    = recv_payload_size;
        recv_shuttle->error_code      = LIBOS_OK;
    }

    kernel_port_continue_send(send_shuttle, send_port);

    __builtin_unreachable();    
}

/**
 *   @brief Handles all port send/recv/wait operations
 * 
 *   Additional inputs in registers 
 *       LIBOS_REG_IOCTL_PORT_WAIT_ID          RISCV_T1
 *       LIBOS_REG_IOCTL_PORT_WAIT_TIMEOUT     RISCV_T2
 *
 *   Outputs in registers:
 *       LIBOS_REG_IOCTL_PORT_WAIT_OUT_SHUTTLE RISCV_A6
 *       LIBOS_REG_IOCTL_PORT_WAIT_RECV_SIZE   RISCV_A4
 *       LIBOS_REG_IOCTL_PORT_WAIT_REMOTE_TASK RISCV_A5
*/
void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN kernel_syscall_port_send_recv(
    LwU64 send_shuttle_id, kernel_handle send_port_id, LwU64 send_payload, LwU64 send_payload_size, 
    LwU64 recv_shuttle_id, kernel_handle recv_port_id, LwU64 recv_payload, LwU64 recv_payload_size)
{
    libos_shuttle_t *send_shuttle = 0;
    libos_shuttle_t *recv_shuttle = 0;
    libos_shuttle_t *wait_shuttle = 0;
    libos_port_t    *send_port = 0;
    libos_port_t    *recv_port = 0;
    LwU64            new_task_state = LIBOS_TASK_STATE_NORMAL;

    if (recv_shuttle_id)
    {
        recv_shuttle = (libos_shuttle_t *)kernel_resource_validate_or_raise_error_to_task(
            recv_shuttle_id, LIBOS_RESOURCE_SHUTTLE_GRANT);
            
        recv_port = (libos_port_t *)kernel_resource_validate_or_raise_error_to_task(
            recv_port_id, LIBOS_RESOURCE_PORT_OWNER_GRANT);

        if (recv_payload_size)
            validate_memory_access_or_return(
                recv_payload, recv_payload_size, LW_TRUE /* request write */);            
    }

    if (send_shuttle_id)
    {
        // Lookup the shuttle
        send_shuttle = (libos_shuttle_t *)kernel_resource_validate_or_raise_error_to_task(
            send_shuttle_id, LIBOS_RESOURCE_SHUTTLE_GRANT);

        if (send_payload_size)
            validate_memory_access_or_return(
                send_payload, send_payload_size, LW_FALSE /* request read */);

        // Can we locate the port?
        if (send_port_id)
        {
            send_port =
                kernel_resource_validate_or_raise_error_to_task(send_port_id, LIBOS_RESOURCE_PORT_SEND_GRANT);            
        }
        // Prior state was either complete or reset, so reply_to_shuttle field must be valid
        else if (send_shuttle->reply_to_shuttle)
        {
            send_port = LIBOS_PTR32_EXPAND(libos_shuttle_t, send_shuttle->reply_to_shuttle)->waiting_on_port;
        }
        else
            kernel_return_access_errorcode();
    }

    // @bug: Can't tell different between "wait on any" and "wait on none"
    LwU16 wait_shuttle_id = self->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_ID];
    if (wait_shuttle_id)
    {        
        if (self->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_TIMEOUT] != LIBOS_TIMEOUT_INFINITE)
            new_task_state = LIBOS_TASK_STATE_PORT_WAIT | LIBOS_TASK_STATE_TIMER_WAIT;
        else
            new_task_state = LIBOS_TASK_STATE_PORT_WAIT;


        if (wait_shuttle_id != LIBOS_SHUTTLE_ANY)
        {
            wait_shuttle = (libos_shuttle_t *)kernel_resource_validate_or_raise_error_to_task(
                wait_shuttle_id, LIBOS_RESOURCE_SHUTTLE_GRANT);
        }
    }    

    kernel_port_send_recv_wait(send_shuttle, send_port, send_payload, send_payload_size,
                   recv_shuttle, recv_port, recv_payload, recv_payload_size,
                   new_task_state, wait_shuttle, self->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_TIMEOUT]);
}

static void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN
kernel_port_continue_send(libos_shuttle_t *send_shuttle, libos_port_t *send_port)
{
    /*
     * Send Shuttle Queue or Copy
     */
    if (send_shuttle)
    {
        /*
         *  Is there a waiting receiver? We must copy immediately
         *  otherwise, we just queue everything.
         */
        if (send_port->waiting_future)
        {
            libos_shuttle_t *waiter    = LIBOS_PTR32_EXPAND(libos_shuttle_t, send_port->waiting_future);
            send_port->waiting_future  = 0;
            send_shuttle->retired_pair = waiter;
            LwU64 copy_size            = send_shuttle->payload_size;
            if (waiter->payload_size < copy_size)
            {
                copy_size          = 0;
                waiter->error_code = send_shuttle->error_code = LIBOS_ERROR_INCOMPLETE;
            }
            waiter->payload_size = copy_size;

            self->continuation       = LIBOS_CONTINUATION_RECV;
            self->kernel_write_pasid = task_by_id[waiter->owner]->pasid;
            self->kernel_read_pasid  = task_by_id[send_shuttle->owner]->pasid;
            kernel_port_copy_and_continue(waiter->payload_address, send_shuttle->payload_address, copy_size);
        }
        else
        {
            // Add this shuttle to the ports waiting senders list
            send_shuttle->port_sender_link.next = LIBOS_PTR32_NARROW(&send_port->waiting_senders);
            send_shuttle->port_sender_link.prev = send_port->waiting_senders.prev;
            list_link(&send_shuttle->port_sender_link);

            // Don't retire the shuttle
            self->sending_shuttle = 0;
        }
    }

    kernel_port_continue_recv();
}

void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN kernel_port_continue_recv()
{
    libos_shuttle_t *send_shuttle_to_retire = LIBOS_PTR32_EXPAND(libos_shuttle_t, self->sending_shuttle);
    libos_shuttle_t *recv_shuttle           = LIBOS_PTR32_EXPAND(libos_shuttle_t, self->receiving_shuttle);

    if (send_shuttle_to_retire)
        kernel_shuttle_retire_pair(send_shuttle_to_retire, send_shuttle_to_retire->retired_pair);

    if (recv_shuttle)
    {
        libos_port_t *recv_port = LIBOS_PTR32_EXPAND(libos_port_t, self->receiving_port);

        // Is there a waiting sender?
        if (LIBOS_PTR32_EXPAND(list_header_t, recv_port->waiting_senders.next) != &recv_port->waiting_senders)
        {
            libos_shuttle_t *waiting_sender =
                container_of(recv_port->waiting_senders.next, libos_shuttle_t, port_sender_link);
            list_unlink(&waiting_sender->port_sender_link);

            recv_shuttle->retired_pair = waiting_sender;
            LwU64 copy_size            = waiting_sender->payload_size;
            if (recv_shuttle->payload_size < copy_size)
            {
                copy_size                = 0;
                recv_shuttle->error_code = waiting_sender->error_code = LIBOS_ERROR_INCOMPLETE;
            }
            recv_shuttle->payload_size = copy_size;

            self->continuation       = LIBOS_CONTINUATION_WAIT;
            self->kernel_write_pasid = task_by_id[recv_shuttle->owner]->pasid;
            self->kernel_read_pasid  = task_by_id[waiting_sender->owner]->pasid;
            kernel_port_copy_and_continue(
                recv_shuttle->payload_address, waiting_sender->payload_address, copy_size);
        }
        else
        {
            // Link into ports waiting senders list
            recv_shuttle->waiting_on_port = recv_port;
            recv_port->waiting_future     = LIBOS_PTR32_NARROW(recv_shuttle);

            // Don't retire the shuttle
            self->receiving_shuttle = 0;
        }
    }

    kernel_port_continue_wait();
}

void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN kernel_port_continue_wait()
{
    libos_shuttle_t *recv_shuttle_to_retire = LIBOS_PTR32_EXPAND(libos_shuttle_t, self->receiving_shuttle);

    if (recv_shuttle_to_retire)
        kernel_shuttle_retire_pair(recv_shuttle_to_retire->retired_pair, recv_shuttle_to_retire);

    // still waiting? We might be waiting on something we didn't just submit
    // and that item might be "complete already"
    if (self->task_state & LIBOS_TASK_STATE_PORT_WAIT)
    {
        libos_shuttle_t *wait = LIBOS_PTR32_EXPAND(libos_shuttle_t, self->sleeping_on_shuttle);

        // @note wait->state cannot be COMPLETE_IDLE as it is checked in libos_port_send_recv
        if (!wait)
        {
            // Any completed shuttles?
            if (LIBOS_PTR32_EXPAND(list_header_t, self->task_completed_shuttles.next) !=
                &self->task_completed_shuttles)
            {
                wait =
                    container_of(self->task_completed_shuttles.next, libos_shuttle_t, shuttle_completed_link);
            }
        }

        // @note wait->state still cannot be COMPLETE_IDLE as that implies it wouldn't have been
        // present in the completed_shuttles list
        if (wait && (wait->state == LIBOS_SHUTTLE_STATE_COMPLETE_PENDING ||
                     wait->state == LIBOS_SHUTTLE_STATE_COMPLETE_IDLE))
        {
            if (wait->state == LIBOS_SHUTTLE_STATE_COMPLETE_PENDING)
            {
                list_unlink(&wait->shuttle_completed_link);
                wait->state = LIBOS_SHUTTLE_STATE_COMPLETE_IDLE;
            }

            kernel_transcribe_status(self, wait);
            kernel_timeout_cancel(self);
            self->task_state = LIBOS_TASK_STATE_NORMAL;
            kernel_return();
        }
        else
        {
            kernel_schedule_wait();
        }        
    }
    else
    {
        // Not in a wait state, just return
        self->state.registers[LIBOS_REG_IOCTL_STATUS_A0] = LIBOS_OK;
    }

    kernel_return();
}
