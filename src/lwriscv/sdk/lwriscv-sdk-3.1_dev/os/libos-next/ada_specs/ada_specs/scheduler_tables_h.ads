pragma Ada_2012;
pragma Style_Checks (Off);

with Interfaces.C; use Interfaces.C;
with lwtypes_h;
with list_h;
with libos_manifest_h;
with libos_task_state_h;

package scheduler_tables_h
   with
      SPARK_Mode => OFF
is

   LIBOS_HASH_MULTIPLIER : constant := 16#72777af862a1faee#;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:173

   LIBOS_XEPC_SENTINEL_NORMAL_TASK_EXIT : constant := 0;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:182

  -- _LWRM_COPYRIGHT_BEGIN_
  -- *
  -- * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
  -- * information contained herein is proprietary and confidential to LWPU
  -- * Corporation.  Any use, reproduction, or disclosure without the written
  -- * permission of LWPU Corporation is prohibited.
  -- *
  -- * _LWRM_COPYRIGHT_END_
  --  

   subtype Pasid is lwtypes_h.LwU8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:18

   type TaskState is 
     (TaskStateReady,
      TaskStateWait,
      TaskStateWaitTimeout,
      TaskStateWaitTrapped)
   with Convention => C;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:25

   type TaskContinuation is 
     (TaskContinuationNone,
      TaskContinuationRecv,
      TaskContinuationWait)
   with Convention => C;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:31

  --*
  -- * @brief Waitable structure
  --  

   type Port is record
      waitingReceive : aliased lwtypes_h.LwU32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:38
      waitingSenders : aliased list_h.ListHeader;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:39
   end record
   with Convention => C_Pass_By_Copy;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:40

  --*
  -- * @brief Port structure
  --  

   type Timer is record
      the_port : aliased Port;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:47
      timerHeader : aliased list_h.ListHeader;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:48
      timestamp : aliased lwtypes_h.LwU64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:49
      pending : aliased lwtypes_h.LwBool;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:50
   end record
   with Convention => C_Pass_By_Copy;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:51

  --(DRF_MASK(LW_RISCV_CSR_MMPUIDX_INDEX) + 1 + 63)/64 
  -- WAIT
  -- SEND
  -- RECV
   type c_Task_array900 is array (0 .. 1) of aliased lwtypes_h.LwU64;
   type c_Task is record
      timestamp : aliased lwtypes_h.LwU64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:55
      schedulerHeader : aliased list_h.ListHeader;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:56
      timerHeader : aliased list_h.ListHeader;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:57
      shuttlesCompleted : aliased list_h.ListHeader;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:58
      the_taskState : aliased TaskState;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:60
      id : aliased libos_manifest_h.LibosTaskId;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:62
      priority : aliased libos_manifest_h.LibosPriority;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:65
      continuation : aliased TaskContinuation;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:72
      state : aliased libos_task_state_h.LibosTaskState;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:74
      portSynchronousReply : aliased Port;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:76
      mpuEnables : aliased c_Task_array900;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:84
      shuttleSleeping : aliased lwtypes_h.LwU32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:88
      shuttleSending : aliased lwtypes_h.LwU32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:91
      shuttleReceiving : aliased lwtypes_h.LwU32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:94
      portReceiving : aliased lwtypes_h.LwU32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:95
      tableKey : aliased lwtypes_h.LwU64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:97
   end record
   with Convention => C_Pass_By_Copy;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:98

   type ObjectGrant is 
     (ObjectGrantNone,
      ObjectGrantWait,
      PortGrantSend,
      PortGrantAll,
      TimerGrantSet,
      TimerGrantAll)
   with Convention => C;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:110

  -- relative to portArray 
   type ObjectTableEntry is record
      offset : aliased lwtypes_h.LwU16;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:114
      owner : aliased libos_manifest_h.LibosTaskId;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:115
      grant : aliased ObjectGrant;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:116
      id : aliased libos_manifest_h.LibosPortId;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:117
   end record
   with Convention => C_Pass_By_Copy;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:118

   type ShuttleState is 
     (ShuttleStateReset,
      ShuttleStateCompleteIdle,
      ShuttleStateCompletePending,
      ShuttleStateSend,
      ShuttleStateRecv)
   with Convention => C;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:126

  -- 32 bytes
  -- state == ShuttleStateSend
  -- During completion
  -- state == LIBOS_SHUTTLE_STATE_COMPLETE (intermediate)
  -- state == LIBOS_SHUTTLE_STATE_COMPLETE (final)
  -- statae == LIBOS_SHUTTLE_RECV
  -- state == ShuttleStateSend
   type Shuttle;
   type anon909_union910 (discr : unsigned := 0) is record
      case discr is
         when 0 =>
            portSenderLink : aliased list_h.ListHeader;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:134
         when 1 =>
            retiredPair : access Shuttle;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:138
         when 2 =>
            shuttleCompletedLink : aliased list_h.ListHeader;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:141
         when others =>
            waitingOnPort : access Port;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:144
      end case;
   end record
   with Convention => C_Pass_By_Copy,
        Unchecked_Union => True;
   type anon909_union914 (discr : unsigned := 0) is record
      case discr is
         when 0 =>
            partnerRecvShuttle : aliased lwtypes_h.LwU32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:150
         when 1 =>
            grantedShuttle : aliased lwtypes_h.LwU32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:153
         when others =>
            replyToShuttle : aliased lwtypes_h.LwU32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:156
      end case;
   end record
   with Convention => C_Pass_By_Copy,
        Unchecked_Union => True;
   type Shuttle is record
      anon1985 : aliased anon909_union910;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:145
      anon1990 : aliased anon909_union914;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:157
      state : aliased ShuttleState;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:159
      originTask : aliased libos_manifest_h.LibosTaskId;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:161
      errorCode : aliased lwtypes_h.LwU8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:163
      owner : aliased libos_manifest_h.LibosTaskId;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:164
      payloadAddress : aliased lwtypes_h.LwU64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:166
      payloadSize : aliased lwtypes_h.LwU32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:167
      taskLocalHandle : aliased libos_manifest_h.LibosPortId;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:168
   end record
   with Convention => C_Pass_By_Copy;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:129

  -- state == ShuttleStateRecv
  -- state == LIBOS_SHUTTLE_STATE_COMPLETE
  -- Large prime
   taskTable : aliased array (size_t) of aliased c_Task  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:175
   with Import => True, 
        Convention => C, 
        External_Name => "taskTable";

   shuttleTable : aliased array (size_t) of aliased Shuttle  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:176
   with Import => True, 
        Convention => C, 
        External_Name => "shuttleTable";

   NotaddrShuttleTableMask : aliased lwtypes_h.LwU8  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:177
   with Import => True, 
        Convention => C, 
        External_Name => "NotaddrShuttleTableMask";

   objectTable : aliased array (size_t) of aliased ObjectTableEntry  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:178
   with Import => True, 
        Convention => C, 
        External_Name => "objectTable";

   NotaddrPortTableMask : aliased lwtypes_h.LwU8  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:179
   with Import => True, 
        Convention => C, 
        External_Name => "NotaddrPortTableMask";

   portArray : aliased array (size_t) of aliased lwtypes_h.LwU8  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/scheduler-tables.h:180
   with Import => True, 
        Convention => C, 
        External_Name => "portArray";

end scheduler_tables_h;
