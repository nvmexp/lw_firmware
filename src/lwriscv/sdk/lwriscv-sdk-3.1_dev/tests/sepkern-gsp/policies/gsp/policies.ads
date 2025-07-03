-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_
with Lw_Types;                use Lw_Types;
with Policy_Types;            use Policy_Types;
with Mpu_Policy_Types;        use Mpu_Policy_Types;
with Iopmp_Policy_Types;      use Iopmp_Policy_Types;
with Device_Map_Policy_Types; use Device_Map_Policy_Types;
with Policy_External;         use Policy_External;
with Dev_Riscv_Csr_64;        use Dev_Riscv_Csr_64;
with Dev_Prgnlcl;             use Dev_Prgnlcl;
with Types;                   use Types;

with Lw_Types.Shift_Right_Op; use Lw_Types.Shift_Right_Op;

package Policies is

-- Helper structures, see to bottom of file to find actual policies

    Core_Pmp_Access_Mode_RWX : constant PMP_Access_Mode := (
        Exelwtion_Access => PERMITTED,
        Write_Access => PERMITTED,
        Read_Access => PERMITTED,
        Reserved => 0
    );

    Core_Pmp_Access_Mode_RX : constant PMP_Access_Mode := (
        Exelwtion_Access => PERMITTED,
        Write_Access => DENIED,
        Read_Access => PERMITTED,
        Reserved => 0
    );

    Core_Pmp_Access_Mode_RW : constant PMP_Access_Mode := (
        Exelwtion_Access => DENIED,
        Write_Access => PERMITTED,
        Read_Access => PERMITTED,
        Reserved => 0
    );

    Core_Pmp_Access_Mode_None : constant PMP_Access_Mode := (
        Exelwtion_Access => DENIED,
        Write_Access => DENIED,
        Read_Access => DENIED,
        Reserved => 0
    );

    Core_Pmp_Entry_Disabled : constant Core_Pmp_Entry := (
        Addr => 16#0#,
        Address_Mode => OFF,
        Access_Mode => Core_Pmp_Access_Mode_None,
        Reserved => 0
    );

    Device_Map_Entry_Access_None : constant SUBMMODE_GROUP := (
        Readable => SUBMMODE_READ_DISABLE,
        Writable => SUBMMODE_WRITE_DISABLE,
        Reserved => 0
    );

    Device_Map_Entry_Access_RO : constant SUBMMODE_GROUP := (
        Readable => SUBMMODE_READ_ENABLE,
        Writable => SUBMMODE_WRITE_DISABLE,
        Reserved => 0
    );

    Device_Map_Entry_Access_WO: constant SUBMMODE_GROUP := (
        Readable => SUBMMODE_READ_DISABLE,
        Writable => SUBMMODE_WRITE_ENABLE,
        Reserved => 0
    );

    Device_Map_Entry_Access_RW: constant SUBMMODE_GROUP := (
        Readable => SUBMMODE_READ_ENABLE,
        Writable => SUBMMODE_WRITE_ENABLE,
        Reserved => 0
    );

-- Those need to be later split for RTOS / non-rtos partition, but let's start simple
    Device_Map_Group_0_Defaults : constant Plcy_Device_Map_Group_0 := (
        MMODE     => Device_Map_Entry_Access_RW,
        RISCV_CTL => Device_Map_Entry_Access_RW,
        PIC       => Device_Map_Entry_Access_RW,
        TIMER     => Device_Map_Entry_Access_RW,
        HOSTIF    => Device_Map_Entry_Access_RW,
        DMA       => Device_Map_Entry_Access_RW,
        PMB       => Device_Map_Entry_Access_RW,
        DIO       => Device_Map_Entry_Access_RW
    );

    Device_Map_Group_1_Defaults : constant Plcy_Device_Map_Group_1 := (
        KEY       => Device_Map_Entry_Access_RW,
        DEBUG     => Device_Map_Entry_Access_RW,
        SHA       => Device_Map_Entry_Access_RW,
        KMEM      => Device_Map_Entry_Access_RW,
        BROM      => Device_Map_Entry_Access_RO, -- brom enforced anyway
        ROM_PATCH => Device_Map_Entry_Access_RW,
        IOPMP     => Device_Map_Entry_Access_RW,
        NOACCESS  => Device_Map_Entry_Access_RW
    );

    Device_Map_Group_2_Defaults : constant Plcy_Device_Map_Group_2 := (
        SCP => Device_Map_Entry_Access_RW,
        FBIF => Device_Map_Entry_Access_RW,
        FALCON_ONLY => Device_Map_Entry_Access_RW,
        PRGN_CTL => Device_Map_Entry_Access_RW,
        SCR_GRP0 => Device_Map_Entry_Access_RW,
        SCR_GRP1 => Device_Map_Entry_Access_RW,
        SCR_GRP2 => Device_Map_Entry_Access_RW,
        SCR_GRP3 => Device_Map_Entry_Access_RW
    );

    Device_Map_Group_3_Defaults : constant Plcy_Device_Map_Group_3 := (
        PLM      => Device_Map_Entry_Access_RW,
        HUB_DIO  => Device_Map_Entry_Access_RW,
        Reserved => 0
    );

    Debug_Control_Defaults_Permissive : constant Plcy_Debug_Control := (
        Debug_Ctrl => LW_PRGNLCL_RISCV_DBGCTL_Register'(
            Icd_Cmdwl_Stop   => STOP_ENABLE,
            Icd_Cmdwl_Run    => RUN_ENABLE,
            Rsvd1            => LW_PRGNLCL_RISCV_DBGCTL_RSVD1_INIT,
            Icd_Cmdwl_Step   => STEP_ENABLE,
            Icd_Cmdwl_J      => J_ENABLE,
            Icd_Cmdwl_Emask  => EMASK_ENABLE,
            Icd_Cmdwl_Rreg   => RREG_ENABLE,
            Icd_Cmdwl_Wreg   => WREG_ENABLE,
            Icd_Cmdwl_Rdm    => RDM_ENABLE,
            Icd_Cmdwl_Wdm    => WDM_ENABLE,
            Icd_Cmdwl_Rstat  => RSTAT_ENABLE,
            Icd_Cmdwl_Ibrkpt => IBRKPT_ENABLE,
            Icd_Cmdwl_Rcsr   => RCSR_ENABLE,
            Icd_Cmdwl_Wcsr   => WCSR_ENABLE,
            Icd_Cmdwl_Rpc    => RPC_ENABLE,
            Icd_Cmdwl_Rfreg  => RFREG_ENABLE,
            Icd_Cmdwl_Wfreg  => WFREG_ENABLE,
            Rsvd             => LW_PRGNLCL_RISCV_DBGCTL_RSVD_INIT,
            Start_In_Icd     => ICD_FALSE,
            Single_Step_Mode => MODE_ENABLE),

        Debug_Ctrl_Lock => LW_PRGNLCL_RISCV_DBGCTL_LOCK_Register'(
            Icd_Cmdwl_Stop   => STOP_UNLOCKED,
            Icd_Cmdwl_Run    => RUN_UNLOCKED,
            Rsvd1            => LW_PRGNLCL_RISCV_DBGCTL_LOCK_RSVD1_INIT,
            Icd_Cmdwl_Step   => STEP_UNLOCKED,
            Icd_Cmdwl_J      => J_UNLOCKED,
            Icd_Cmdwl_Emask  => EMASK_UNLOCKED,
            Icd_Cmdwl_Rreg   => RREG_UNLOCKED,
            Icd_Cmdwl_Wreg   => WREG_UNLOCKED,
            Icd_Cmdwl_Rdm    => RDM_UNLOCKED,
            Icd_Cmdwl_Wdm    => WDM_UNLOCKED,
            Icd_Cmdwl_Rstat  => RSTAT_UNLOCKED,
            Icd_Cmdwl_Ibrkpt => IBRKPT_UNLOCKED,
            Icd_Cmdwl_Rcsr   => RCSR_UNLOCKED,
            Icd_Cmdwl_Wcsr   => WCSR_UNLOCKED,
            Icd_Cmdwl_Rpc    => RPC_UNLOCKED,
            Icd_Cmdwl_Rfreg  => RFREG_UNLOCKED,
            Icd_Cmdwl_Wfreg  => WFREG_UNLOCKED,
            Rsvd             => LW_PRGNLCL_RISCV_DBGCTL_LOCK_RSVD_INIT,
            Single_Step_Mode => MODE_UNLOCKED)
    );

    Io_Pmp_Entry_Default : constant Io_Pmp_Entry := (
        Cfg  => Plcy_Io_Pmp_Cfg'(
            Read     => READ_DISABLE,
            Write    => WRITE_DISABLE,
            Fbdma    => DISABLE,
            Cpdma    => DISABLE,
            Sha      => DISABLE,
            Reserved => 0,
            PMB0     => DISABLE,
            PMB1     => DISABLE,
            PMB2     => DISABLE,
            PMB3     => DISABLE,
            PMB4     => DISABLE,
            PMB5     => DISABLE,
            PMB6     => DISABLE,
            PMB7     => DISABLE,
            Lock     => LOCK_UNLOCKED),
        Mode            => OFF,
        Addr_Lo_1k      => 0,
        Addr_Lo_Above1k => 0,
        Addr_Hi         => 0
    );

    Io_Pmp_Policy_Default : constant Plcy_Io_Pmp := (
        0 => Io_Pmp_Entry_Default,
        1 => Io_Pmp_Entry_Default,
        2 => Io_Pmp_Entry_Default,
        3 => Io_Pmp_Entry_Default,
        4 => Io_Pmp_Entry_Default,
        5 => Io_Pmp_Entry_Default,
        6 => Io_Pmp_Entry_Default,
        7 => Io_Pmp_Entry_Default
    );
--------------------------------------------------------------------------------
-- Actual policies
--------------------------------------------------------------------------------
   Always_Clear_MPU_On_Switch : constant LwU1 := 0;

   Partition_RTOS_Core_Pmp : constant Plcy_Core_Pmp := (
        -- All IMEM allocated to RTOS (most of it)
        Core_Pmp_Entry'(Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0010_3000#), 2)),
                            Address_Mode => OFF,
                            Access_Mode => Core_Pmp_Access_Mode_None,
                            Reserved => 0),
        Core_Pmp_Entry'(Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0012_4000#), 2)),
                            Address_Mode => TOR,
                            Access_Mode => Core_Pmp_Access_Mode_RWX, -- RWX because bootloader writes, to be fixed for prod scenarios
                            Reserved => 0),
        -- All DMEM allocated to RTOS (most of it, including debug buffer)
        Core_Pmp_Entry'(Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_3000#), 2)),
                            Address_Mode => OFF,
                            Access_Mode => Core_Pmp_Access_Mode_None,
                            Reserved => 0),
        Core_Pmp_Entry'(Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0019_0000#), 2)),
                            Address_Mode => TOR,
                            Access_Mode => Core_Pmp_Access_Mode_RW,
                            Reserved => 0),
        -- FB - RWX because bootloader :(
        Core_Pmp_Entry'(Addr => PMP_Address(Shift_Right(LwU64(16#0606_00000_0000_0000#), 2)),
                            Address_Mode => OFF,
                            Access_Mode => Core_Pmp_Access_Mode_None,
                            Reserved => 0),
        Core_Pmp_Entry'(Addr => PMP_Address(Shift_Right(LwU64(16#0606_F0000_0000_0000#), 2)),
                            Address_Mode => TOR,
                            Access_Mode => Core_Pmp_Access_Mode_RWX,
                            Reserved => 0),
        -- Sysmem, it's enough to have RW (if anything)
        Core_Pmp_Entry'(Addr => PMP_Address(Shift_Right(LwU64(16#0806_00000_0000_0000#), 2)),
                            Address_Mode => OFF,
                            Access_Mode => Core_Pmp_Access_Mode_None,
                            Reserved => 0),
        Core_Pmp_Entry'(Addr => PMP_Address(Shift_Right(LwU64(16#0806_F0000_0000_0000#), 2)),
                            Address_Mode => TOR,
                            Access_Mode => Core_Pmp_Access_Mode_RW,
                            Reserved => 0),
        -- EMEM, needed for communication with CPU
        Core_Pmp_Entry'(Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0120_0000#), 2)),
                            Address_Mode => OFF,
                            Access_Mode =>Core_Pmp_Access_Mode_None,
                            Reserved => 0),
        Core_Pmp_Entry'(Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0120_2000#), 2)),
                            Address_Mode => TOR,
                            Access_Mode => Core_Pmp_Access_Mode_RW,
                            Reserved => 0),
        Core_Pmp_Entry_Disabled,
        Core_Pmp_Entry_Disabled,
        Core_Pmp_Entry_Disabled,
        Core_Pmp_Entry_Disabled,
        Core_Pmp_Entry_Disabled,
        Core_Pmp_Entry_Disabled
    );

   Partition_Selwre_Core_Pmp : constant Plcy_Core_Pmp := (
       -- IMEM carveout
       Core_Pmp_Entry'(Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0010_2000#), 2)),
                           Address_Mode => OFF,
                           Access_Mode => Core_Pmp_Access_Mode_None,
                           Reserved => 0),
       Core_Pmp_Entry'(Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0012_3000#), 2)),
                           Address_Mode => TOR,
                           Access_Mode => Core_Pmp_Access_Mode_RWX, -- RWX because bootloader writes, to be fixed for prod scenarios
                           Reserved => 0),
       -- DMEM carveout
       Core_Pmp_Entry'(Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_2000#), 2)),
                           Address_Mode => OFF,
                           Access_Mode => Core_Pmp_Access_Mode_None,
                           Reserved => 0),
       Core_Pmp_Entry'(Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_3000#), 2)),
                           Address_Mode => TOR,
                           Access_Mode => Core_Pmp_Access_Mode_RW,
                           Reserved => 0),
       -- print buffer
       Core_Pmp_Entry'(Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_F000#), 2)),
                           Address_Mode => OFF,
                           Access_Mode => Core_Pmp_Access_Mode_None,
                           Reserved => 0),
       Core_Pmp_Entry'(Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0019_0000#), 2)),
                           Address_Mode => TOR,
                           Access_Mode => Core_Pmp_Access_Mode_RW,
                           Reserved => 0),
       Core_Pmp_Entry_Disabled,
       Core_Pmp_Entry_Disabled,
       Core_Pmp_Entry_Disabled,
       Core_Pmp_Entry_Disabled,
       Core_Pmp_Entry_Disabled,
       Core_Pmp_Entry_Disabled,
       Core_Pmp_Entry_Disabled,
       Core_Pmp_Entry_Disabled,
       Core_Pmp_Entry_Disabled,
       Core_Pmp_Entry_Disabled
    );


-------------------------------------------------------------------------------------------------------------------------------
--***************************************************************************************************************************--
-------------------------------------------------------------------------------------------------------------------------------
    Partition_ID_RTOS       : constant Partition_ID := 0;
    Partition_ID_Selwre     : constant Partition_ID := 1;

    Partition_RTOS_Policy : constant External_Policy := (
        Switchable_To       => Plcy_Switchable_To_Array'(
                                Partition_ID_Selwre => PERMITTED,
                                others => DENIED),
        Entry_Point_Address => 16#00010_3000#,
        Ucode_Id            => 4,
        Sspm                => Plcy_SSPM'(
                                Splm => 10#5#,
                                Ssecm => SSECM_INSEC),
        Secret_Mask         => Plcy_Secret_Mask'(
                                Scp_Secret_Mask0 => 0,
                                Scp_Secret_Mask1 => 0,
                                Scp_Secret_Mask_lock0 => 0,
                                Scp_Secret_Mask_lock1 => 0),
        Debug_Control       => Debug_Control_Defaults_Permissive,
        Device_Map_Group_0  => Device_Map_Group_0_Defaults,
        Device_Map_Group_1  => Device_Map_Group_1_Defaults,
        Device_Map_Group_2  => Device_Map_Group_2_Defaults,
        Device_Map_Group_3  => Device_Map_Group_3_Defaults,
        Core_Pmp            => Partition_RTOS_Core_Pmp,
        Io_Pmp              => Io_Pmp_Policy_Default,
        Mpu_Control         => Plcy_Mpu_Control'(
                                Start_Index       => 0,
                                Entry_Count       => 128)
        );


    Partition_Selwre_Policy : constant External_Policy := (
        Switchable_To       => Plcy_Switchable_To_Array'(
                                Partition_ID_RTOS => PERMITTED,
                                others => DENIED),
        Entry_Point_Address => 16#00010_2000#,
        Ucode_Id            => 4,
        Sspm                => Plcy_SSPM'(
                                Splm => 2#1#,
                                Ssecm => SSECM_INSEC),
        Secret_Mask         => Plcy_Secret_Mask'(
                                Scp_Secret_Mask0 => 0,
                                Scp_Secret_Mask1 => 0,
                                Scp_Secret_Mask_lock0 => 0,
                                Scp_Secret_Mask_lock1 => 0),
        Debug_Control       => Debug_Control_Defaults_Permissive,
        Device_Map_Group_0  => Device_Map_Group_0_Defaults,
        Device_Map_Group_1  => Device_Map_Group_1_Defaults,
        Device_Map_Group_2  => Device_Map_Group_2_Defaults,
        Device_Map_Group_3  => Device_Map_Group_3_Defaults,
        Core_Pmp            => Partition_Selwre_Core_Pmp,
        Io_Pmp              => Io_Pmp_Policy_Default,
        Mpu_Control         => Plcy_Mpu_Control'(
                                Start_Index       => 0,
                                Entry_Count       => 0)
    );
   -------------------------------------------------------------------------------------------------------------------------------
   --***************************************************************************************************************************--
   -------------------------------------------------------------------------------------------------------------------------------
    External_Policies : constant External_Policy_Array := External_Policy_Array'(
        Partition_ID_RTOS   => Partition_RTOS_Policy,
        Partition_ID_Selwre => Partition_Selwre_Policy
    );
end Policies;
