#include "peregrine-headers.h"

KernelSchedulerRunnable = . ;
  LONG(KernelSchedulerRunnable);
  LONG(KernelSchedulerRunnable);

dispatch_table = .;
  /* This constant comes from trap.c:
      see: move top bit to DISPATCH_INTERRUPT */
  #define DISPATCH_INTERRUPT 32
  #define DISPATCH(i, v) . = dispatch_table + (i) * 4; LONG(v);
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_IAMA, KernelTaskPanic);
#if LIBOS_FEATURE_PAGING
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_IACC_FAULT, kernel_codefault);
#else
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_IACC_FAULT, KernelTaskPanic);
#endif
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_ILL, KernelTaskPanic);
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_BKPT, KernelTaskPanic);
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_LAMA, KernelTaskPanic);
#if LIBOS_FEATURE_PAGING && (LIBOS_LWRISCV < 200)
  /* Peregrine 1.x reports MPU translation failures as LACC faults */
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_LACC_FAULT, kernel_datafault);
#else
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_LACC_FAULT, KernelTaskPanic);
#endif
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_SAMA, KernelTaskPanic);

#if LIBOS_FEATURE_PAGING && (LIBOS_LWRISCV < 200)
  /* Peregrine 1.x reports MPU translation failures as SACC faults */
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_SACC_FAULT, kernel_datafault);
#else
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_SACC_FAULT, KernelTaskPanic);
#endif
#if LIBOS_FEATURE_PAGING && LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_IPAGE_FAULT, kernel_codefault);
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_LPAGE_FAULT, kernel_datafault);
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_SPAGE_FAULT, kernel_datafault);
#elif LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_IPAGE_FAULT, KernelTaskPanic);
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_LPAGE_FAULT, KernelTaskPanic);
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_SPAGE_FAULT, KernelTaskPanic);
#endif
  /* If we could prove that USS and SSS weren't reachable, we could save 64 bytes */
  DISPATCH(LW_RISCV_CSR_MCAUSE_EXCODE_SSS, 0)

#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
  DISPATCH(DISPATCH_INTERRUPT | LW_RISCV_CSR_MCAUSE_EXCODE_S_TINT, KernelSchedulerInterrupt)
#else
  DISPATCH(DISPATCH_INTERRUPT | LW_RISCV_CSR_MCAUSE_EXCODE_M_TINT, KernelSchedulerInterrupt)
#endif

#if LIBOS_CONFIG_EXTERNAL_INTERRUPT || LIBOS_CONFIG_INTEGRITY_MEMERR
#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
  DISPATCH(DISPATCH_INTERRUPT | LW_RISCV_CSR_MCAUSE_EXCODE_S_EINT, KernelExternalInterrupt)
#else
  DISPATCH(DISPATCH_INTERRUPT | LW_RISCV_CSR_MCAUSE_EXCODE_M_EINT, KernelExternalInterrupt)
#endif
#endif


ucall_dispatch_table = ABSOLUTE(.) ;
  LONG(PortSyscallSendRecvWait);
  LONG(KernelSchedulerPreempt);
#if LIBOS_FEATURE_PARTITION_SWITCH
  LONG(KernelSyscallPartitionSwitch);
#endif

  LONG(KernelSyscallCacheIlwalidate);
  LONG(ShuttleSyscallReset);

  /* DMA functions not wired */
  LONG(KernelTaskPanic);
  LONG(KernelTaskPanic);
#if LIBOS_FEATURE_PAGING
  LONG(kernel_syscall_init_register_pagetables);
#else
  LONG(KernelSyscallBootstrapMmap);
#endif
#if LIBOS_CONFIG_PROFILER_SAVE_ON_SUSPEND
  LONG(kernel_syscall_profiler_enable);
#endif
#if LIBOS_CONFIG_EXTERNAL_INTERRUPT
  LONG(KernelSyscallExternalInterruptArm);
#endif
  LONG(KernelSyscallTaskCriticalSectionEnter);
  LONG(KernelSyscallTaskCriticalSectionLeave);
#if LIBOS_FEATURE_SHUTDOWN
  LONG(KernelSbiShutdown);
#endif
#if LIBOS_FEATURE_TIMERS
  LONG(TimerSyscallTimerSet);
#endif
#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
  LONG(KernelSyscallGdmaTransfer);
  LONG(KernelSyscallGdmaFlush);
#endif
  LONG(KernelSyscallLockRelease)
