pragma Ada_2012;
pragma Style_Checks (Off);

with Interfaces.C; use Interfaces.C;
with System;
with lwtypes_h;
with libos_status_h;

package libos_h is

   --  arg-macro: function CONTAINER_OF (p, type, field)
   --    return (type *)((LwU64)(p)-offsetof(type, field));
   LIBOS_PARTITION_SWITCH_ARG_COUNT : constant := 5;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:55

   LIBOS_MPU_CODE : constant := 0;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:156
   LIBOS_MPU_DATA_INIT_AND_KERNEL : constant := 1;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:157
   LIBOS_MPU_MMIO_KERNEL : constant := 2;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:158
   LIBOS_MPU_USER_INDEX : constant := 3;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:159
   --  arg-macro: procedure LIBOS_TASK_PANIC_NAKED ()
   --    __asm__ __volatile__( "li t0, %[IMM_SYSCALL_TASK_PANIC];" & "ecall;" :: (IMM_SYSCALL_TASK_PANIC) & "i"(LIBOS_SYSCALL_TASK_PANIC) )

  -- _LWRM_COPYRIGHT_BEGIN_
  -- *
  -- * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
  -- * information contained herein is proprietary and confidential to LWPU
  -- * Corporation.  Any use, reproduction, or disclosure without the written
  -- * permission of LWPU Corporation is prohibited.
  -- *
  -- * _LWRM_COPYRIGHT_END_
  --  

  --*
  -- * @brief Flushes GSP's DCACHE
  -- *
  -- * @note This call has no effect on PAGED_TCM mappings.
  -- *
  -- * @param base
  -- *     Start of memory region to ilwalidate
  -- * @param size
  -- *     Total length of the region to ilwalidate in bytes
  --  

   procedure LibosCacheIlwalidate (base : System.Address; size : lwtypes_h.LwU64)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:39
   with Import => True, 
        Convention => C, 
        External_Name => "LibosCacheIlwalidate";

  --*
  -- * @brief Yield the GSP to next KernelSchedulerRunnable task
  --  

   procedure LibosTaskYield  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:45
   with Import => True, 
        Convention => C, 
        External_Name => "LibosTaskYield";

  --*
  -- * @brief Put the GSP into the suspended state
  -- *
  -- * This call issues a writeback and ilwalidate to the calling task's
  -- * page entries prior to notifying the host CPU that the GSP is
  -- * entering the suspended state.
  --  

   procedure LibosPartitionSwitch
     (partitionId : lwtypes_h.LwU64;
      inArgs : access lwtypes_h.LwU64;
      outArgs : access lwtypes_h.LwU64)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:57
   with Import => True, 
        Convention => C, 
        External_Name => "LibosPartitionSwitch";

   procedure LibosInitHookMpu  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:67
   with Import => True, 
        Convention => C, 
        External_Name => "LibosInitHookMpu";

   function LibosInitDaemon return lwtypes_h.LwU32  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:68
   with Import => True, 
        Convention => C, 
        External_Name => "LibosInitDaemon";

  --*
  -- * @brief Use DMA to transfer data from a DMA buffer to/from a TCM buffer.
  -- *
  -- * The DMA buffer should be declared with the DMA memory region attribute.
  -- * The TCM buffer should be declared with the PAGED_TCM memory region attribute.
  -- *
  -- * @param[in] tcmVa    Virtual address of TCM buffer
  -- * @param[in] dmaVA    Virtual address of DMA buffer
  -- * @param[in] size     Number of bytes to transfer
  -- * @param[in] from_tcm LW_FALSE - transfer from DMA buffer to TCM buffer
  -- *                     LW_TRUE  - transfer from TCM buffer to DMA buffer
  -- *
  -- * @note The tcmVa, dmaVA and size arguments must all be 4-byte aligned.
  -- *
  -- * @returns
  -- *    LibosErrorAccess
  -- *      One of the input arguments is not 4-byte aligned.
  -- *    LibosErrorArgument
  -- *      dmaSrc argument does not represent a valid memory region.
  -- *      The underlying physical memory of the DMA buffer is not in a valid sysmem or fbmem aperture.
  -- *    LibosErrorFailed
  -- *      The destination TCM buffer could not be accessed.
  -- *    LibosOk
  -- *      The DMA transfer was successful.
  --  

   function LibosDmaCopy
     (tcmVa : lwtypes_h.LwU64;
      dmaVA : lwtypes_h.LwU64;
      size : lwtypes_h.LwU64;
      fromTcm : lwtypes_h.LwBool) return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:96
   with Import => True, 
        Convention => C, 
        External_Name => "LibosDmaCopy";

  --*
  -- * @brief Issue a DMA flush operation.
  -- *
  -- * This call waits for all pending DMA operations to complete before returning.
  --  

   procedure LibosDmaFlush  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:103
   with Import => True, 
        Convention => C, 
        External_Name => "LibosDmaFlush";

  --*
  -- * @brief Use GDMA to transfer data.
  --  

   function LibosGdmaTransfer
     (dstVa : lwtypes_h.LwU64;
      srcVa : lwtypes_h.LwU64;
      size : lwtypes_h.LwU32;
      flags : lwtypes_h.LwU64) return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:110
   with Import => True, 
        Convention => C, 
        External_Name => "LibosGdmaTransfer";

  --*
  -- * @brief Issue a GDMA flush operation.
  --  

   function LibosGdmaFlush return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:115
   with Import => True, 
        Convention => C, 
        External_Name => "LibosGdmaFlush";

  --*
  -- * @brief Register ilwerted page-table (init task only)
  -- *
  -- * This call waits for all pending DMA operations to complete before returning.
  --  

  --*
  -- * @brief Enable GSP profiler and set up the hardware profiling counters.
  --  

  --*
  -- * @brief KernelBootstrap a mapping into the MPU/TLB
  -- *
  -- * @note This call may only be used in init before the scheduler
  -- *       and paging subsystems have been enabled.
  -- *
  -- *       All addresses and sizes must be a multiple of
  -- *       LIBOS_CONFIG_MPU_PAGESIZE.
  -- *
  -- * @param va
  -- *     Virtual address of start of mapping. No collision detection
  -- *     will be performed.
  -- * @param pa
  -- *     Physical address to map
  -- * @param size
  -- *     Length of mapping.
  -- * @param attributes
  -- *     Processor specific MPU attributes or TLB attributes for mapping.
  --  

   procedure LibosBootstrapMmap
     (index : lwtypes_h.LwU64;
      va : lwtypes_h.LwU64;
      pa : lwtypes_h.LwU64;
      size : lwtypes_h.LwU64;
      attributes : lwtypes_h.LwU64)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:161
   with Import => True, 
        Convention => C, 
        External_Name => "LibosBootstrapMmap";

  --*
  -- * @brief Exit the current task such that it will no longer run.
  -- * 
  -- * The exit code will be available to TASK_INIT.
  -- * This syscall does not return.
  -- * 
  -- * Note: this syscall was made with voluntary task termination in mind
  -- * although it lwrrently has the same effect as LibosPanic (and is
  -- * handled by the trap handling in TASK_INIT).
  -- * 
  -- * @param[in] exitCode   Exit code
  --  

   procedure LibosTaskExit (exitCode : lwtypes_h.LwU64)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:178
   with Import => True, 
        Convention => C, 
        External_Name => "LibosTaskExit";

  --*
  -- * @brief Panic the current task such that it will no longer run.
  -- * 
  -- * The panic reason will be available to TASK_INIT's task trap handling.
  -- * This syscall does not return.
  -- * 
  -- * Note: this syscall is also available via a function-like macro wrapper
  -- * (that only uses basic asm) to allow for its use in attribute((naked))
  -- * functions.
  -- * 
  -- * @param[in] reason   Reason for panic
  --  

  -- function-like macro wrapper for LibosPanic
   procedure LibosPanic  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:208
   with Import => True, 
        Convention => C, 
        External_Name => "LibosPanic";

  -- Queue a receive on the interrupt port
  -- Reset and clear any prior interrupts to avoid race where we miss the interrupt
  --*
  -- * @brief Enters a scheduler critical section
  -- * 
  -- * Once in a scheduler critical section the kernel will not context switch
  -- * to another task on end of timeslice.  The kernel interrupt ISR is still 
  -- * enabled but all user-space interrupt servicing is disabled.
  -- * 
  -- * Ports:: Waits on shuttle completion are forbidden.  Asynchronous send/recv
  -- *         is allowed.
  -- * 
  -- * 
  -- * @param[in] behavior   Controls whether the critical section is released on 
  -- *                       task crash.
  --  

   type LibosCriticalSectionBehavior is 
     (LibosCriticalSectionNone,
      LibosCriticalSectionPanicOnTrap,
      LibosCriticalSectionReleaseOnTrap)
   with Convention => C;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:252

   procedure LibosCriticalSectionEnter (behavior : LibosCriticalSectionBehavior)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:254
   with Import => True, 
        Convention => C, 
        External_Name => "LibosCriticalSectionEnter";

  --*
  -- * @brief Leaves a scheduler critical section
  --  

   procedure LibosCriticalSectionLeave  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:259
   with Import => True, 
        Convention => C, 
        External_Name => "LibosCriticalSectionLeave";

   procedure LibosProcessorShutdown  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos.h:261
   with Import => True, 
        Convention => C, 
        External_Name => "LibosProcessorShutdown";

end libos_h;
