/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBOS_SYSCALLS_H_
#define LIBOS_SYSCALLS_H_
#include "libos-config.h"
#include "../kernel/lwriscv.h"

#define ROUND_POWER_2()

/*
    CAUTION! All syscalls must be present when not operating in !LIBOS_TINY
    
    This ensures consistent SYSCALL numbering for user-space code (ELFs).
*/
typedef enum {
    /**
     * @brief Scheduler system call interface.  
     * 
     */
    LIBOS_SYSCALL_TASK_PORT_SEND_RECV_WAIT,
    LIBOS_SYSCALL_TASK_YIELD,
    LIBOS_SYSCALL_SWITCH_TO,
    LIBOS_SYSCALL_DCACHE_ILWALIDATE,            // Not technically a scheduler syscall
    LIBOS_SYSCALL_SHUTTLE_RESET,
    LIBOS_SYSCALL_INTERRUPT_BIND,
    LIBOS_SYSCALL_INTERRUPT_ARM_EXTERNAL,
    LIBOS_SYSCALL_TIMER_SET,
    LIBOS_SYSCALL_TASK_CRITICAL_SECTION_ENTER,
    LIBOS_SYSCALL_TASK_CRITICAL_SECTION_LEAVE,
    LIBOS_SYSCALL_LOCK_RELEASE,

    /**
     * @brief LIBOS_TINY syscalls 
     * 
     * @note: LIBOS full API's are built on top of ports in server.c
     *        and are scheduled.
     * 
     */
#if !LIBOS_TINY
    LIBOS_SYSCALL_SHUTTLE_CREATE,       // Must be a syscall since shuttles are aware of their own handle
    LIBOS_SYSCALL_PROFILER_ENABLE,
    LIBOS_SYSCALL_PARTITION_CREATE,
    LIBOS_SYSCALL_PARTITION_MEMORY_GRANT_PHYSICAL,
    LIBOS_SYSCALL_PARTITION_MEMORY_ALLOCATE,
    LIBOS_SYSCALL_PARTITION_SPAWN,
    LIBOS_SYSCALL_HANDLE_CLOSE,
    LIBOS_SYSCALL_REGION_MAP_GENERIC,
    LIBOS_SYSCALL_LOCK_CREATE,    
    LIBOS_SYSCALL_PARTITION_EXIT,
    LIBOS_SYSCALL_PHYSICAL_MEMORY_SET_ALLOCATE,
    LIBOS_SYSCALL_PHYSICAL_MEMORY_SET_INSERT,
    LIBOS_SYSCALL_PHYSICAL_MEMORY_NODE_ALLOCATE,
#else
    LIBOS_SYSCALL_BOOTSTRAP_MMAP,
#endif

#if !LIBOS_TINY || LIBOS_FEATURE_SHUTDOWN
    LIBOS_SYSCALL_SHUTDOWN,
#endif

#if !LIBOS_TINY || LIBOS_CONFIG_GDMA_SUPPORT
    LIBOS_SYSCALL_GDMA_TRANSFER,
    LIBOS_SYSCALL_GDMA_FLUSH,
#endif

    LIBOS_SYSCALL_LIMIT,

    // These syscalls are special in that they always fail and kill the task
    // They are treated as invalid syscalls by the RTOS 
    LIBOS_SYSCALL_TASK_PANIC,
    LIBOS_SYSCALL_TASK_EXIT,
} libos_syscall_id_t;

typedef enum {
#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
    LIBOS_DAEMON_SWITCH_TO,
#else
    LIBOS_DAEMON_SUSPEND_PROCESSOR,
#endif
} libos_daemon_port_id_t;

#define LIBOS_REG_IOCTL_A0        RISCV_A0
#define LIBOS_REG_IOCTL_STATUS_A0 RISCV_A0

#define LIBOS_REG_IOCTL_PORT_WAIT_ID          RISCV_T1
#define LIBOS_REG_IOCTL_PORT_WAIT_TIMEOUT     RISCV_T2
#define LIBOS_REG_IOCTL_PORT_FLAGS            RISCV_T3
#define LIBOS_REG_IOCTL_PORT_WAIT_OUT_SHUTTLE RISCV_A6
#define LIBOS_REG_IOCTL_PORT_WAIT_RECV_SIZE   RISCV_A4

#ifndef LIBOS_SYSCALL_DIRECT_DISPATCH
#define LIBOS_SYSCALL_INSTRUCTION  "ecall"
#else
#define LIBOS_SYSCALL_INSTRUCTION  "jal gp, KernelSchedulerSyscallFromKernel"
#endif

#endif
