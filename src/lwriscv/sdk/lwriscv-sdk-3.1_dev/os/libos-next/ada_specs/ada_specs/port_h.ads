pragma Ada_2012;
pragma Style_Checks (Off);

with Interfaces.C; use Interfaces.C;
with libos_manifest_h;
with lwtypes_h;
with System;
limited with scheduler_tables_h;

package port_h is

  -- _LWRM_COPYRIGHT_BEGIN_
  -- *
  -- * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
  -- * information contained herein is proprietary and confidential to LWPU
  -- * Corporation.  Any use, reproduction, or disclosure without the written
  -- * permission of LWPU Corporation is prohibited.
  -- *
  -- * _LWRM_COPYRIGHT_END_
  --  

  -- Object table for manifest
   function ObjectFindOrRaiseErrorToTask (arg1 : libos_manifest_h.LibosPortId; arg2 : lwtypes_h.LwU64) return System.Address  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/port.h:17
   with Import => True, 
        Convention => C, 
        External_Name => "ObjectFindOrRaiseErrorToTask";

  -- SYSCALL 
   procedure PortSyscallSendRecvWait
     (sendShuttleId : libos_manifest_h.LibosShuttleId;
      sendPortId : libos_manifest_h.LibosPortId;
      sendPayload : lwtypes_h.LwU64;
      sendPayloadSize : lwtypes_h.LwU64;
      recvShuttleId : libos_manifest_h.LibosShuttleId;
      replyPortId : libos_manifest_h.LibosPortId;
      recvPayload : lwtypes_h.LwU64;
      recvPayloadSize : lwtypes_h.LwU64)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/port.h:20
   with Import => True, 
        Convention => C, 
        External_Name => "PortSyscallSendRecvWait";

   procedure PortSendRecvWait
     (sendShuttle : access scheduler_tables_h.Shuttle;
      sendPort : access scheduler_tables_h.Port;
      recvShuttle : access scheduler_tables_h.Shuttle;
      recvPort : access scheduler_tables_h.Port;
      newTaskState : lwtypes_h.LwU64;
      waitShuttle : access scheduler_tables_h.Shuttle;
      waitTimeout : lwtypes_h.LwU64)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/port.h:25
   with Import => True, 
        Convention => C, 
        External_Name => "PortSendRecvWait";

  -- Required by task.c
   procedure PortContinueRecv  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/port.h:31
   with Import => True, 
        Convention => C, 
        External_Name => "PortContinueRecv";

   procedure PortContinueWait  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/port.h:32
   with Import => True, 
        Convention => C, 
        External_Name => "PortContinueWait";

   function ShuttleIsIdle (the_shuttle : access scheduler_tables_h.Shuttle) return lwtypes_h.LwBool  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/port.h:34
   with Import => True, 
        Convention => C, 
        External_Name => "ShuttleIsIdle";

   procedure KernelShuttleReset (resetShuttle : access scheduler_tables_h.Shuttle)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/port.h:35
   with Import => True, 
        Convention => C, 
        External_Name => "KernelShuttleReset";

   procedure ShuttleBindSend
     (sendShuttle : access scheduler_tables_h.Shuttle;
      sendPayload : lwtypes_h.LwU64;
      sendPayloadSize : lwtypes_h.LwU64)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/port.h:37
   with Import => True, 
        Convention => C, 
        External_Name => "ShuttleBindSend";

   procedure ShuttleBindGrant (sendShuttle : access scheduler_tables_h.Shuttle; recvShuttle : access scheduler_tables_h.Shuttle)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/port.h:38
   with Import => True, 
        Convention => C, 
        External_Name => "ShuttleBindGrant";

   procedure ShuttleBindRecv
     (recvShuttle : access scheduler_tables_h.Shuttle;
      recvPayload : lwtypes_h.LwU64;
      recvPayloadSize : lwtypes_h.LwU64)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/port.h:39
   with Import => True, 
        Convention => C, 
        External_Name => "ShuttleBindRecv";

   procedure ShuttleRetireSingle (the_shuttle : access scheduler_tables_h.Shuttle)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/port.h:40
   with Import => True, 
        Convention => C, 
        External_Name => "ShuttleRetireSingle";

end port_h;
