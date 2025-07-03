-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types;                use Lw_Types;
with Types;                   use Types;
with Policy_Types;            use Policy_Types;
with Policy_External_Types;   use Policy_External_Types;
with Mpu_Policy_Types;        use Mpu_Policy_Types;
with Device_Map_Policy_Types; use Device_Map_Policy_Types;
with Dev_Riscv_Csr_64;        use Dev_Riscv_Csr_64;
with Dev_Prgnlcl;             use Dev_Prgnlcl;
with Iopmp_Policy_Types;      use Iopmp_Policy_Types;

package Policy_External
is
    type External_Policy is new External_Policy_Type;
    type External_Policy_Array is array (Partition_ID) of External_Policy with
            Dynamic_Predicate => (
                                  -- Check debug ctrl lock setting:
                                  -- Not locked on any policy
                                  -- OR
                                  -- If locked on any policy, all partitions must have same setting.
                                  (for all I in Partition_ID =>
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Icd_Cmdwl_Stop = STOP_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Stop = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Stop))) and then
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Icd_Cmdwl_Run = RUN_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Run = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Run))) and then
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Icd_Cmdwl_Step = STEP_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Step = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Step))) and then
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Icd_Cmdwl_J = J_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Icd_Cmdwl_J = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Icd_Cmdwl_J))) and then
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Icd_Cmdwl_Emask = EMASK_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Emask = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Emask))) and then
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Icd_Cmdwl_Rreg = RREG_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Rreg = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Rreg))) and then
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Icd_Cmdwl_Wreg = WREG_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Wreg = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Wreg))) and then
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Icd_Cmdwl_Rdm = RDM_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Rdm = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Rdm))) and then
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Icd_Cmdwl_Wdm = WDM_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Wdm = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Wdm))) and then
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Icd_Cmdwl_Rstat = RSTAT_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Rstat = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Rstat))) and then
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Icd_Cmdwl_Ibrkpt = IBRKPT_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Ibrkpt = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Ibrkpt))) and then
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Icd_Cmdwl_Rcsr = RCSR_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Rcsr = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Rcsr))) and then
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Icd_Cmdwl_Wcsr = WCSR_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Wcsr = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Wcsr))) and then
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Icd_Cmdwl_Rpc = RPC_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Rpc = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Rpc))) and then
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Icd_Cmdwl_Rfreg = RFREG_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Rfreg = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Rfreg))) and then
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Icd_Cmdwl_Wfreg = WFREG_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Wfreg = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Icd_Cmdwl_Wfreg))) and then
                                  ((External_Policy_Array(I).Debug_Control.Debug_Ctrl_Lock.Single_Step_Mode = MODE_UNLOCKED) or else
                                   (for all J in Partition_ID => (External_Policy_Array(I).Debug_Control.Debug_Ctrl.Single_Step_Mode = External_Policy_Array(J).Debug_Control.Debug_Ctrl.Single_Step_Mode))))

                                   and then
                                  -- Check Secure Partition MMODE setting:
                                  -- If Secure partition feature is on, MMODE must be blocked for S/U mode write access
                                  (for all I in Partition_ID =>
                                  (External_Policy_Array(I).Device_Map_Group_0.MMODE.Writable = MMODE_WRITE_DISABLE))
                                   and then
                                  -- Check IOPMP lock setting:
                                  -- Not locked on any entries
                                  -- OR
                                  -- If locked on any policy, all partitions must have same settings for the locked index
                                 (for all I in Partition_ID => (for all index in Io_Pmp_Array_Index => (External_Policy_Array(I).Io_Pmp(index).Cfg.Lock = LOCK_UNLOCKED) or else
                                                                 (for all J in Partition_ID => (External_Policy_Array(J).Io_Pmp(index).Mode = External_Policy_Array(I).Io_Pmp(index).Mode) and then
                                                                                               (External_Policy_Array(J).Io_Pmp(index).Cfg.Read = External_Policy_Array(I).Io_Pmp(index).Cfg.Read) and then
                                                                                               (External_Policy_Array(J).Io_Pmp(index).Cfg.Write = External_Policy_Array(I).Io_Pmp(index).Cfg.Write) and then
                                                                                               (External_Policy_Array(J).Io_Pmp(index).Cfg.Fbdma = External_Policy_Array(I).Io_Pmp(index).Cfg.Fbdma) and then
                                                                                               (External_Policy_Array(J).Io_Pmp(index).Cfg.Cpdma = External_Policy_Array(I).Io_Pmp(index).Cfg.Cpdma) and then
                                                                                               (External_Policy_Array(J).Io_Pmp(index).Cfg.Sha = External_Policy_Array(I).Io_Pmp(index).Cfg.Sha) and then
                                                                                               (External_Policy_Array(J).Io_Pmp(index).Cfg.PMB0 = External_Policy_Array(I).Io_Pmp(index).Cfg.PMB0) and then
                                                                                               (External_Policy_Array(J).Io_Pmp(index).Cfg.PMB1 = External_Policy_Array(I).Io_Pmp(index).Cfg.PMB1) and then
                                                                                               (External_Policy_Array(J).Io_Pmp(index).Cfg.PMB2 = External_Policy_Array(I).Io_Pmp(index).Cfg.PMB2) and then
                                                                                               (External_Policy_Array(J).Io_Pmp(index).Cfg.PMB3 = External_Policy_Array(I).Io_Pmp(index).Cfg.PMB3) and then
                                                                                               (External_Policy_Array(J).Io_Pmp(index).Cfg.PMB4 = External_Policy_Array(I).Io_Pmp(index).Cfg.PMB4) and then
                                                                                               (External_Policy_Array(J).Io_Pmp(index).Cfg.PMB5 = External_Policy_Array(I).Io_Pmp(index).Cfg.PMB5) and then
                                                                                               (External_Policy_Array(J).Io_Pmp(index).Cfg.PMB6 = External_Policy_Array(I).Io_Pmp(index).Cfg.PMB6) and then
                                                                                               (External_Policy_Array(J).Io_Pmp(index).Cfg.PMB7 = External_Policy_Array(I).Io_Pmp(index).Cfg.PMB7) and then
                                                                                               (External_Policy_Array(J).Io_Pmp(index).Addr_Lo_Above1k = External_Policy_Array(I).Io_Pmp(index).Addr_Lo_Above1k) and then
                                                                                               (External_Policy_Array(J).Io_Pmp(index).Addr_Hi = External_Policy_Array(I).Io_Pmp(index).Addr_Hi) and then
                                                                                               (External_Policy_Array(J).Io_Pmp(index).Addr_Lo_1k = External_Policy_Array(I).Io_Pmp(index).Addr_Lo_1k)))));

end Policy_External;
