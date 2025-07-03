pragma Ada_2012;
pragma Style_Checks (Off);

with Interfaces.C; use Interfaces.C;
limited with scheduler_tables_h;
with lwtypes_h;

package timer_h is

  -- _LWRM_COPYRIGHT_BEGIN_
  -- *
  -- * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
  -- * information contained herein is proprietary and confidential to LWPU
  -- * Corporation.  Any use, reproduction, or disclosure without the written
  -- * permission of LWPU Corporation is prohibited.
  -- *
  -- * _LWRM_COPYRIGHT_END_
  --  

   procedure KernelTimerInit  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/timer.h:16
   with Import => True, 
        Convention => C, 
        External_Name => "KernelTimerInit";

   procedure KernelTimerLoad  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/timer.h:17
   with Import => True, 
        Convention => C, 
        External_Name => "KernelTimerLoad";

   procedure KernelTimerEnque (target : access scheduler_tables_h.c_Task)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/timer.h:18
   with Import => True, 
        Convention => C, 
        External_Name => "KernelTimerEnque";

   procedure KernelTimerCancel (target : access scheduler_tables_h.c_Task)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/timer.h:19
   with Import => True, 
        Convention => C, 
        External_Name => "KernelTimerCancel";

   function KernelTimerProcessElapsed (mtime : lwtypes_h.LwU64) return lwtypes_h.LwU64  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/timer.h:20
   with Import => True, 
        Convention => C, 
        External_Name => "KernelTimerProcessElapsed";

   procedure KernelTimerArmInterrupt (nextPreempt : lwtypes_h.LwU64)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/timer.h:21
   with Import => True, 
        Convention => C, 
        External_Name => "KernelTimerArmInterrupt";

   procedure KernelTimerSet (the_timer : access scheduler_tables_h.Timer; timestamp : lwtypes_h.LwU64)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/timer.h:22
   with Import => True, 
        Convention => C, 
        External_Name => "KernelTimerSet";

end timer_h;
