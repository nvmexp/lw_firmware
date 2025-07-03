pragma Ada_2012;
pragma Style_Checks (Off);

with Interfaces.C; use Interfaces.C;

package libos_syscalls_h is

   --  unsupported macro: LIBOS_REG_IOCTL_A0 RISCV_A0
   --  unsupported macro: LIBOS_REG_IOCTL_STATUS_A0 RISCV_A0
   --  unsupported macro: LIBOS_REG_IOCTL_PORT_WAIT_ID RISCV_T1
   --  unsupported macro: LIBOS_REG_IOCTL_PORT_WAIT_TIMEOUT RISCV_T2
   --  unsupported macro: LIBOS_REG_IOCTL_PORT_WAIT_OUT_SHUTTLE RISCV_A6
   --  unsupported macro: LIBOS_REG_IOCTL_PORT_WAIT_RECV_SIZE RISCV_A4
   --  unsupported macro: LIBOS_REG_IOCTL_PORT_WAIT_REMOTE_TASK RISCV_A5
   --  unsupported macro: LIBOS_REG_IOCTL_MEMORY_QUERY_ACCESS_PERMS RISCV_A1
   --  unsupported macro: LIBOS_REG_IOCTL_MEMORY_QUERY_ALLOCATION_BASE RISCV_A2
   --  unsupported macro: LIBOS_REG_IOCTL_MEMORY_QUERY_ALLOCATION_SIZE RISCV_A3
   --  unsupported macro: LIBOS_REG_IOCTL_MEMORY_QUERY_APERTURE RISCV_A4
   --  unsupported macro: LIBOS_REG_IOCTL_MEMORY_QUERY_PHYSICAL_OFFSET RISCV_A5
   --  unsupported macro: LIBOS_MEMORY_ACCESS_MASK_READ U(1)
   --  unsupported macro: LIBOS_MEMORY_ACCESS_MASK_WRITE U(2)
   --  unsupported macro: LIBOS_MEMORY_ACCESS_MASK_EXELWTE U(4)
   --  unsupported macro: LIBOS_MEMORY_APERTURE_FB U(0)
   --  unsupported macro: LIBOS_MEMORY_APERTURE_SYSCOH U(1)
   --  unsupported macro: LIBOS_MEMORY_APERTURE_MMIO U(2)
  -- _LWRM_COPYRIGHT_BEGIN_
  -- *
  -- * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
  -- * information contained herein is proprietary and confidential to LWPU
  -- * Corporation.  Any use, reproduction, or disclosure without the written
  -- * permission of LWPU Corporation is prohibited.
  -- *
  -- * _LWRM_COPYRIGHT_END_
  --  

  -- These syscalls are special in that they always fail and kill the task
  -- They are treated as invalid syscalls by the RTOS 
   type libos_syscall_id_t is 
     (LIBOS_SYSCALL_TASK_PORT_SEND_RECV_WAIT,
      LIBOS_SYSCALL_TASK_YIELD,
      LIBOS_SYSCALL_SWITCH_TO,
      LIBOS_SYSCALL_DCACHE_ILWALIDATE,
      LIBOS_SYSCALL_SHUTTLE_RESET,
      LIBOS_SYSCALL_TASK_DMA_COPY,
      LIBOS_SYSCALL_TASK_DMA_FLUSH,
      LIBOS_SYSCALL_BOOTSTRAP_MMAP,
      LIBOS_SYSCALL_TASK_CRITICAL_SECTION_ENTER,
      LIBOS_SYSCALL_TASK_CRITICAL_SECTION_LEAVE,
      LIBOS_SYSCALL_SHUTDOWN,
      LIBOS_SYSCALL_TIMER_SET,
      LIBOS_SYSCALL_GDMA_TRANSFER,
      LIBOS_SYSCALL_GDMA_FLUSH,
      LIBOS_SYSCALL_LIMIT,
      LIBOS_SYSCALL_TASK_PANIC,
      LIBOS_SYSCALL_TASK_EXIT)
   with Convention => C;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_syscalls.h:60

   type libos_daemon_port_id_t is 
     (LIBOS_DAEMON_SWITCH_TO)
   with Convention => C;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_syscalls.h:68

  -- LIBOS_SYSCALL_MEMORY_QUERY register layout for argument handling.
  -- LIBOS_SYSCALL_MEMORY_QUERY access values.
  -- LIBOS_SYSCALL_MEMORY_QUERY aperture values.
end libos_syscalls_h;
