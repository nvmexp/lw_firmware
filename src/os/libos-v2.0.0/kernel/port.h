/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBOS_PORT_H
#define LIBOS_PORT_H

#include "kernel.h"

typedef LwU8 task_port_index_t;
typedef LwU8 task_index_t;

// @todo Make a generic linked list type
typedef list_header_t libos_shuttle_list_header_t;

/**
 * @brief Port structure
 *      16 bytes
 */
typedef struct
{

    /**
     *  @note Relies on port_or_scheduler_link.m_next and allowed_waiting aliasing.
     *
     *  allowed_waiter->task_state != LIBOS_TASK_STATE_BLOCKED_SEND_PORT
     *      implies allowed_waiter->[RISCV_A0] contains the port we're waiting on
     *  otherwise:
     *      port_or_scheduler_link forms a linked list of waiting senders.
     */

    // @note blocked senders
    libos_shuttle_list_header_t waiting_senders;
    LIBOS_PTR32(libos_shuttle_t) waiting_future;

} libos_port_t;

// 32 bytes
typedef struct libos_shuttle_t
{
    union
    {
        // state == LIBOS_SHUTTLE_STATE_SEND
        libos_shuttle_list_header_t port_sender_link;

        // During completion
        // state == LIBOS_SHUTTLE_STATE_COMPLETE (intermediate)
        struct libos_shuttle_t *retired_pair;

        // state == LIBOS_SHUTTLE_STATE_COMPLETE (final)
        libos_shuttle_list_header_t shuttle_completed_link;

        // statae == LIBOS_SHUTTLE_RECV
        libos_port_t *waiting_on_port;
    };

    union
    {
        // state == LIBOS_SHUTTLE_STATE_SEND
        LIBOS_PTR32(libos_shuttle_t) partner_recv_shuttle;

        // state == LIBOS_SHUTTLE_STATE_RECV
        LIBOS_PTR32(libos_shuttle_t) granted_shuttle;

        // state == LIBOS_SHUTTLE_STATE_COMPLETE
        LIBOS_PTR32(libos_shuttle_t) reply_to_shuttle;
    };

    LwU8 state;
    LwU8 origin_task;

    LwU8 error_code;
    task_index_t owner;

    LwU64 payload_address;
    LwU32 payload_size;
    LwU32 task_local_port_handle;

} libos_shuttle_t;

#define LIBOS_SHUTTLE_STATE_RESET            0
#define LIBOS_SHUTTLE_STATE_COMPLETE_IDLE    1
#define LIBOS_SHUTTLE_STATE_COMPLETE_PENDING 2
#define LIBOS_SHUTTLE_STATE_SEND             3
#define LIBOS_SHUTTLE_STATE_RECV             4

void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN kernel_port_send_recv_wait(
    libos_shuttle_t * send_shuttle, libos_port_t * send_port, LwU64 send_payload, LwU64 send_payload_size, 
    libos_shuttle_t * recv_shuttle, libos_port_t * recv_port, LwU64 recv_payload, LwU64 recv_payload_size,
    LwU64 new_task_state, libos_shuttle_t * wait_shuttle, LwU64 wait_timeout);

void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN kernel_syscall_port_send_recv(
    LwU64 send_shuttle_id, kernel_handle send_port_id, LwU64 send_payload, LwU64 send_payload_size, 
    LwU64 recv_shuttle_id, kernel_handle reply_port_id, LwU64 recv_payload, LwU64 recv_payload_size);

// Required by task.c
void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN kernel_port_continue_recv(void);
void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN kernel_port_continue_wait(void);

#endif
