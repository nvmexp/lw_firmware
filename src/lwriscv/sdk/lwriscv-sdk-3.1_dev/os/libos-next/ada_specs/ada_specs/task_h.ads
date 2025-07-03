pragma Ada_2012;
pragma Style_Checks (Off);

with Interfaces.C; use Interfaces.C;
with scheduler_tables_h;
with libos_h;
with libos_status_h;
with libos_manifest_h;
with lwtypes_h;
limited with list_h;

package task_h is

  -- _LWRM_COPYRIGHT_BEGIN_
  -- *
  -- * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
  -- * information contained herein is proprietary and confidential to LWPU
  -- * Corporation.  Any use, reproduction, or disclosure without the written
  -- * permission of LWPU Corporation is prohibited.
  -- *
  -- * _LWRM_COPYRIGHT_END_
  --  

   resumeTask : access scheduler_tables_h.c_Task  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:20
   with Import => True, 
        Convention => C, 
        External_Name => "resumeTask";

   criticalSection : aliased libos_h.LibosCriticalSectionBehavior  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:21
   with Import => True, 
        Convention => C, 
        External_Name => "criticalSection";

   TaskInit : aliased scheduler_tables_h.c_Task  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:23
   with Import => True, 
        Convention => C, 
        External_Name => "TaskInit";

   procedure KernelTaskPanic  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:25
   with Import => True, 
        Convention => C, 
        External_Name => "KernelTaskPanic";

   procedure KernelSyscallReturn (status : libos_status_h.LibosStatus)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:27
   with Import => True, 
        Convention => C, 
        External_Name => "KernelSyscallReturn";

   procedure KernelTaskInit  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:29
   with Import => True, 
        Convention => C, 
        External_Name => "KernelTaskInit";

   function ShuttleSyscallReset (reset_shuttle_id : libos_manifest_h.LibosShuttleId) return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:30
   with Import => True, 
        Convention => C, 
        External_Name => "ShuttleSyscallReset";

   procedure KernelPortTimeout (owner : access scheduler_tables_h.c_Task)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:31
   with Import => True, 
        Convention => C, 
        External_Name => "KernelPortTimeout";

   procedure kernel_task_replay (task_index : lwtypes_h.LwU32)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:32
   with Import => True, 
        Convention => C, 
        External_Name => "kernel_task_replay";

   procedure ListLink (id : access list_h.ListHeader)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:33
   with Import => True, 
        Convention => C, 
        External_Name => "ListLink";

   procedure ListUnlink (id : access list_h.ListHeader)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:34
   with Import => True, 
        Convention => C, 
        External_Name => "ListUnlink";

   procedure kernel_port_signal  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:35
   with Import => True, 
        Convention => C, 
        External_Name => "kernel_port_signal";

   procedure KernelSyscallPartitionSwitch  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:37
   with Import => True, 
        Convention => C, 
        External_Name => "KernelSyscallPartitionSwitch";

   procedure KernelSyscallTaskCriticalSectionEnter  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:38
   with Import => True, 
        Convention => C, 
        External_Name => "KernelSyscallTaskCriticalSectionEnter";

   procedure KernelSyscallTaskCriticalSectionLeave  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:39
   with Import => True, 
        Convention => C, 
        External_Name => "KernelSyscallTaskCriticalSectionLeave";

   userXStatus : aliased lwtypes_h.LwU64  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:41
   with Import => True, 
        Convention => C, 
        External_Name => "userXStatus";

   procedure KernelTaskReturn  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/task.h:43
   with Import => True, 
        Convention => C, 
        External_Name => "KernelTaskReturn";

end task_h;
