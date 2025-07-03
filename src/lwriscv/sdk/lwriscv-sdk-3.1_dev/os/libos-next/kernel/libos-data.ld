#include "peregrine-headers.h"

KernelSchedulerRunnable = . ;
  LONG(KernelSchedulerRunnable);
  LONG(KernelSchedulerRunnable);

/* Include the interrupt table */
#include "trap_ulwectored.ld"

ucall_dispatch_table = ABSOLUTE(.) ;
  LONG(PortSyscallSendRecvWait);
  LONG(KernelSchedulerPreempt);
  LONG(KernelSyscallPartitionSwitch);
  LONG(KernelSyscallCacheIlwalidate);
  LONG(ShuttleSyscallReset);
  LONG(KernelSyscallExternalInterruptBind)
  LONG(KernelSyscallExternalInterruptArm);
  LONG(TimerSyscallTimerSet)
  LONG(KernelSyscallTaskCriticalSectionEnter);
  LONG(KernelSyscallTaskCriticalSectionLeave);
  LONG(KernelSyscallLockRelease)

#if !LIBOS_TINY
  LONG(KernelSyscallShuttleCreate)
  LONG(kernel_syscall_profiler_enable);
  LONG(KernelSyscallPartitionCreate)
  LONG(KernelSyscallPartitionMemoryGrantPhysical)
  LONG(KernelSyscallPartitionMemoryAllocate)
  LONG(KernelSyscallPartitionSpawn)
  LONG(KernelSyscallHandleClose)
  LONG(KernelSyscallRegionMapGeneric)
  LONG(KernelSyscallLockCreate)  
  LONG(KernelSyscallPartitionExit)
  LONG(KernelSyscallMemorySetAllocate)
  LONG(KernelSyscallMemorySetInsert)
  LONG(KernelSyscallMemoryPoolAllocate)
#else
  LONG(KernelSyscallBootstrapMmap);
#endif

#if  !LIBOS_TINY || LIBOS_FEATURE_SHUTDOWN
  LONG(SeparationKernelShutdown);
#endif

#if  LIBOS_CONFIG_GDMA_SUPPORT
  LONG(KernelSyscallGdmaTransfer);
  LONG(KernelSyscallGdmaFlush);
#elif !LIBOS_TINY
  LONG(KernelTaskPanic)
  LONG(KernelTaskPanic)
#endif

