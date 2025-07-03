/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_TASK_H
#define LIBOS_TASK_H

#include "../include/libos_ilwerted_pagetable.h"
#include "../include/libos_task_state.h"
#include "kernel.h"
#include "dma.h"
#include "port.h"
#include "resource.h"

#define container_of(p, type, field) ((type *)((LwU64)(p)-offsetof(type, field)))

static inline LwU64 kernel_get_time_ns(void)
{
    LwU64 val;
    __asm __volatile__("csrr %[val], time" : [ val ] "=r"(val) : : "memory");
    return LIBOS_CONFIG_TIME_TO_NS(val);
}

//
//  CAUTION!  Layout of structures must match initialization in
//            libos.py/compile_ldscript()
//
typedef struct libos_task_tag
{
    LwU64 timestamp;
    list_header_t task_header_scheduler;
    list_header_t task_header_timer;
    libos_shuttle_list_header_t task_completed_shuttles;

    LwU8 task_state;
    LwU8 timer_pending;
    task_index_t task_id;
    task_port_index_t port_recv;

    libos_pasid_t pasid;
    LwU8 continuation;
    LwU8 reserved[2];

    LibosTaskState state;
    LwU16 kernel_read_pasid;
    LwU16 kernel_write_pasid;
    LwU32 resource_count;

    task_dma_state_t dma;

    // valid on LIBOS_TASK_STATE_PORT_COPY_WAIT
    LIBOS_PTR32(libos_shuttle_t) sleeping_on_shuttle;

    // Used in state LIBOS_TASK_STATE_PORT_COPY_SEND
    LIBOS_PTR32(libos_shuttle_t) sending_shuttle;

    // Used in state LIBOS_TASK_STATE_PORT_COPY_RECV
    LIBOS_PTR32(libos_shuttle_t) receiving_shuttle;
    LIBOS_PTR32(libos_port_t) receiving_port;

    libos_resource_table_entry resources[1 /* variable length array */];
} libos_task_t;

register libos_task_t *self asm("tp");

extern libos_task_t *resume_task;

extern libos_task_t task_init_instance;

void                kernel_syscall_task_query(LwU64 task_index, LibosTaskState *payload);
LIBOS_NORETURN void kernel_task_panic(void);

void                                        kernel_timeout_queue(libos_task_t *target);
void                                        kernel_timeout_cancel(libos_task_t *target);
void                                        kernel_timer_init(void);
void                                        kernel_timer_load(void);
LIBOS_NORETURN void                         kernel_syscall_task_yield(void);
void                                        kernel_task_init(void);
void                                        kernel_sched_init(void);
void LIBOS_NORETURN                         kernel_return_to_task(libos_task_t *new_self);
void LIBOS_NORETURN                         kernel_return(void);
LIBOS_NORETURN void                         kernel_trap(void);
LIBOS_NORETURN void                         kernel_raw_return_to_task(libos_task_t *return_to);
void LIBOS_NORETURN                         kernel_syscall_shuttle_reset(LwU64 reset_shuttle_id);
void                                        kernel_port_timeout(libos_task_t *owner);
void                                        kernel_task_replay(LwU32 task_index);
void                                        list_link(list_header_t *id);
void                                        list_unlink(list_header_t *id);
void                                        kernel_schedule_enque(libos_task_t *id);
void LIBOS_NORETURN                         kernel_schedule_wait(void);
libos_task_t *                              kernel_reschedule_and_deque(void);
void LIBOS_NORETURN                         kernel_port_signal(void);
LIBOS_SECTION_TEXT_COLD LIBOS_NORETURN void kernel_return_access_errorcode(void);

// Task id->task object table
extern libos_task_t *task_by_id[];
extern LwU32 pasid_debug_mask_by_id[];
LIBOS_DECLARE_LINKER_SYMBOL_AS_INTEGER(task_by_id_count);
LIBOS_SECTION_TEXT_COLD libos_task_t *kernel_task_resolve_or_return_errorcode(task_index_t task);

// Task syscalls
LIBOS_NORETURN void kernel_syscall_task_memory_read(
    LwU64        localAddress,  // a0
    task_index_t task,          // a1
    LwU64        remoteAddress, // a2
    LwU64        length         // a3
);

LIBOS_NORETURN void kernel_syscall_processor_suspend();

extern LwU64 user_xstatus;

#endif
