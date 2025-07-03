/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBOS_SYSCALLS_H_
#define LIBOS_SYSCALLS_H_

#ifndef LIBOS_KERNEL
#    ifndef U
#        define U(x)   (x##u)
#        define ULL(x) (x##ull)
#    endif
#endif

// Syscall definitions
#define LIBOS_SYSCALL_ILWALID                  U(0)
#define LIBOS_SYSCALL_TASK_PORT_SEND_RECV_WAIT U(1)
#define LIBOS_SYSCALL_TASK_YIELD               U(2)
#define LIBOS_SYSCALL_PROCESSOR_SUSPEND        U(3)
#define LIBOS_SYSCALL_DCACHE_ILWALIDATE        U(4)
#define LIBOS_SYSCALL_MEMORY_QUERY             U(5)
#define LIBOS_SYSCALL_TASK_STATE_QUERY         U(6)
#define LIBOS_SYSCALL_SHUTTLE_RESET            U(7)
#define LIBOS_SYSCALL_TASK_DMA_REGISTER_TCM    U(8)
#define LIBOS_SYSCALL_TASK_DMA_COPY            U(9)
#define LIBOS_SYSCALL_TASK_DMA_FLUSH           U(10)
#define LIBOS_SYSCALL_TASK_MEMORY_READ         U(11)
#define LIBOS_SYSCALL_INIT_REGISTER_PAGETABLES U(12)
#define LIBOS_SYSCALL_PROFILER_ENABLE          U(13)
#define LIBOS_SYSCALL_TASK_EXIT                U(14)
#define LIBOS_SYSCALL_TASK_PANIC               U(15)
#define LIBOS_SYSCALL_INTERRUPT_ARM_EXTERNAL   U(16)
#define LIBOS_SYSCALL_TIMER_GET_NS             U(17)
#define LIBOS_SYSCALL_DCACHE_ILWALIDATE_ALL    U(18)
#define LIBOS_SYSCALL_PROFILER_GET_COUNTER_MASK U(19)
#define LIBOS_SYSCALL_PARTITION_SWITCH         U(20)
#define LIBOS_SYSCALL_LIMIT_POW2               U(32)

#define LIBOS_REG_IOCTL_A0        RISCV_A0
#define LIBOS_REG_IOCTL_STATUS_A0 RISCV_A0

#define LIBOS_REG_IOCTL_PORT_WAIT_ID          RISCV_T1
#define LIBOS_REG_IOCTL_PORT_WAIT_TIMEOUT     RISCV_T2
#define LIBOS_REG_IOCTL_PORT_WAIT_OUT_SHUTTLE RISCV_A6
#define LIBOS_REG_IOCTL_PORT_WAIT_RECV_SIZE   RISCV_A4
#define LIBOS_REG_IOCTL_PORT_WAIT_REMOTE_TASK RISCV_A5

//
// LIBOS_SYSCALL_MEMORY_QUERY register layout for argument handling.
//
#define LIBOS_REG_IOCTL_MEMORY_QUERY_ACCESS_ATTRS    RISCV_A1 /* out */
#define LIBOS_REG_IOCTL_MEMORY_QUERY_ALLOCATION_BASE RISCV_A2 /* out */
#define LIBOS_REG_IOCTL_MEMORY_QUERY_ALLOCATION_SIZE RISCV_A3 /* out */
#define LIBOS_REG_IOCTL_MEMORY_QUERY_APERTURE        RISCV_A4 /* out */
#define LIBOS_REG_IOCTL_MEMORY_QUERY_PHYSICAL_OFFSET RISCV_A5 /* out */

//
// LIBOS_SYSCALL_MEMORY_QUERY access attributes values.
//
#define LIBOS_MEMORY_ACCESS_MASK_READ        U(1)
#define LIBOS_MEMORY_ACCESS_MASK_WRITE       U(2)
#define LIBOS_MEMORY_ACCESS_MASK_EXELWTE     U(4)
#define LIBOS_MEMORY_ACCESS_MASK_CACHED      U(8)
#define LIBOS_MEMORY_ACCESS_MASK_PAGED_TCM   U(16)

//
// LIBOS_SYSCALL_MEMORY_QUERY aperture values.
//
#define LIBOS_MEMORY_APERTURE_FB     U(0)
#define LIBOS_MEMORY_APERTURE_SYSCOH U(1)
#define LIBOS_MEMORY_APERTURE_MMIO   U(2)

#endif
