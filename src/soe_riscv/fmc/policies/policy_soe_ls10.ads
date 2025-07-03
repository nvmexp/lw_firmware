-- _LWRM_COPYRIGHT_BEGIN_
--
-- Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
-- information contained herein is proprietary and confidential to LWPU
-- Corporation.  Any use, reproduction, or disclosure without the written
-- permission of LWPU Corporation is prohibited.
--
-- _LWRM_COPYRIGHT_END_

with Dev_Prgnlcl;             use Dev_Prgnlcl;
with Dev_Riscv_Csr_64;        use Dev_Riscv_Csr_64;
with Iopmp_Policy_Types;      use Iopmp_Policy_Types;
with Mpu_Policy_Types;        use Mpu_Policy_Types;
with Device_Map_Policy_Types; use Device_Map_Policy_Types;
with Lw_Types.Shift_Right_Op; use Lw_Types.Shift_Right_Op;
with Lw_Types;                use Lw_Types;
with Policy_External;         use Policy_External;
with Policy_Types;            use Policy_Types;
with Types;                   use Types;

package Policies is
    Always_Clear_MPU_On_Switch : constant LwU1 := 1;

    Default_Debug_Control : constant Plcy_Debug_Control := Plcy_Debug_Control'(
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
        Single_Step_Mode => MODE_UNLOCKED));

    Default_Mpu_Control : constant Plcy_Mpu_Control := Plcy_Mpu_Control'(
        Hash_LWMPU_Enable => 0,
        Start_Index       => 0,
        Entry_Count       => 128);

    Default_Device_Map_Group_0 : constant Plcy_Device_Map_Group_0 := (
    MMODE => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
    RISCV_CTL => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
    PIC => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
    TIMER => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
    HOSTIF => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
    DMA => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
    PMB => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
    DIO => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0));

    Default_Device_Map_Group_1 : constant Plcy_Device_Map_Group_1 := (
        KEY => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
        DEBUG => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
        SHA => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
        KMEM => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
        BROM => (
           Readable => SUBMMODE_READ_ENABLE, -- because to read DMA config
           Writable => SUBMMODE_WRITE_DISABLE,
           Reserved => 0),
        ROM_PATCH => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
        IOPMP => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
        NOACCESS => (
           Readable => SUBMMODE_READ_DISABLE,
           Writable => SUBMMODE_WRITE_DISABLE,
           Reserved => 0));

    Default_Device_Map_Group_2 : constant Plcy_Device_Map_Group_2 := (
        SCP => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
        FBIF => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
        FALCON_ONLY => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
        PRGN_CTL => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
        SCR_GRP0 => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
        SCR_GRP1 => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
        SCR_GRP2 => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
        SCR_GRP3 => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0));

    Default_Device_Map_Group_3 : constant Plcy_Device_Map_Group_3 := (
        PLM => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
        HUB_DIO => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
       RESET => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_ENABLE,
           Reserved => 0),
       READ_ONLY => (
           Readable => SUBMMODE_READ_ENABLE,
           Writable => SUBMMODE_WRITE_DISABLE,
           Reserved => 0),
        Reserved => 0);

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

    Core_Pmp_Access_Mode_RWX : constant PMP_Access_Mode := (
        Exelwtion_Access => PERMITTED,
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

    Core_Pmp_Entry_LocalIo_Start : constant Core_Pmp_Entry := (
        Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0140_0000#), 2)),
        Address_Mode => OFF,
        Access_Mode => Core_Pmp_Access_Mode_None,
        Reserved => 0
    );

    Core_Pmp_Entry_LocalIo_End : constant Core_Pmp_Entry := (
        Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0160_0000#), 2)),
        Address_Mode => TOR,
        Access_Mode => Core_Pmp_Access_Mode_RW,
        Reserved => 0
    );

    Core_Pmp_Entry_Pri_Start : constant Core_Pmp_Entry := (
        Addr => PMP_Address(Shift_Right(LwU64(16#2000_0000_0000_0000#), 2)),
        Address_Mode => OFF,
        Access_Mode => Core_Pmp_Access_Mode_None,
        Reserved => 0
    );

    Core_Pmp_Entry_Pri_End : constant Core_Pmp_Entry := (
        Addr => PMP_Address(Shift_Right(LwU64(16#2000_0000_0400_0000#), 2)),
        Address_Mode => TOR,
        Access_Mode => Core_Pmp_Access_Mode_RW,
        Reserved => 0
    );

    Core_Pmp_Entry_Emem_Start : constant Core_Pmp_Entry := (
        Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0120_0000#), 2)),
        Address_Mode => OFF,
        Access_Mode => Core_Pmp_Access_Mode_None,
        Reserved => 0
    );

    Core_Pmp_Entry_Emem_End : constant Core_Pmp_Entry := (
        Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0120_2000#), 2)),
        Address_Mode => TOR,
        Access_Mode => Core_Pmp_Access_Mode_RW,
        Reserved => 0
    );

    -- helpers to define shared regions

    Core_Pmp_Entry_print_shr_Start : constant Core_Pmp_Entry  := (
        Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_4400#), 2)),
        Address_Mode => OFF,
        Access_Mode => Core_Pmp_Access_Mode_None,
        Reserved => 0
    );

    Core_Pmp_Entry_print_shr_End : constant Core_Pmp_Entry  := (
        Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_4C00#), 2)),
        Address_Mode => TOR,
        Access_Mode => Core_Pmp_Access_Mode_RW,
        Reserved => 0
    );

    RTOS_Core_Pmp : constant Plcy_Core_Pmp := (
        Core_Pmp_Entry_Pri_Start,
        Core_Pmp_Entry_Pri_End,
        Core_Pmp_Entry_print_shr_Start,
        Core_Pmp_Entry_print_shr_End,
        -- EMEM
        Core_Pmp_Entry_Emem_Start,
        Core_Pmp_Entry_Emem_End,
        -- IMEM
        Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0010_1000#), 2)),
                                Address_Mode => OFF,
                                Access_Mode => Core_Pmp_Access_Mode_None,
                                Reserved => 0),
        Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0013_0000#), 2)), -- IMEM => 0x30000,     # (bytes) [192 kB] Size of physical IMEM
                                Address_Mode => TOR,
                                Access_Mode => Core_Pmp_Access_Mode_RX,
                                Reserved => 0),
        -- DMEM
        Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_1000#), 2)),
                                Address_Mode => OFF,
                                Access_Mode => Core_Pmp_Access_Mode_None,
                                Reserved => 0),
        Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_001A_0000#), 2)), -- DMEM => 0x20000,     # (bytes) [128 kB] Size of physical DMEM
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

    Default_Io_Pmp_Entry : constant Io_Pmp_Entry := (
        Cfg  => Plcy_Io_Pmp_Cfg'(
            Read         => READ_DISABLE,
            Write        => WRITE_DISABLE,
            Fbdma_Imem   => DISABLE,
            Fbdma_Dmem   => DISABLE,
            Sha          => DISABLE,
            Cpdma        => DISABLE,
            PMB0         => DISABLE,
            PMB1         => DISABLE,
            PMB2         => DISABLE,
            PMB3         => DISABLE,
            Gdma_Chan0_0 => DISABLE,
            Gdma_Chan0_1 => DISABLE,
            Gdma_Chan1_0 => DISABLE,
            Gdma_Chan1_1 => DISABLE,
            Gdma_Chan2_0 => DISABLE,
            Gdma_Chan2_1 => DISABLE,
            Gdma_Chan3_0 => DISABLE,
            Gdma_Chan3_1 => DISABLE,
            SE_AES_0     => DISABLE,
            SE_AES_1     => DISABLE,
            SE_HASH      => DISABLE,
            Lock         => LOCK_UNLOCKED),
        Mode            => OFF, -- policy disabled
        Addr_Lo_1k      => 0,
        Addr_Lo_Above1k => 0,
        Addr_Hi         => 0);

    Default_Io_Pmp : constant Plcy_Io_Pmp := (
        0 => Default_Io_Pmp_Entry,
        1 => Default_Io_Pmp_Entry,
        2 => Default_Io_Pmp_Entry,
        3 => Default_Io_Pmp_Entry,
        4 => Default_Io_Pmp_Entry,
        5 => Default_Io_Pmp_Entry,
        6 => Default_Io_Pmp_Entry,
        7 => Default_Io_Pmp_Entry
       );

    -------------------------------------------------------------------------------------------------------------------------------
    --***************************************************************************************************************************--
    -------------------------------------------------------------------------------------------------------------------------------
    -- Partition indices. WARNING: they must match partitions.h
    Partition_ID_RTOS       : constant Partition_ID := 0;

    -- RTOS partition
    P_RTOS_Policy : constant External_Policy := (
        Switchable_To       => Plcy_Switchable_To_Array'(
                                    others => DENIED),
        Entry_Point_Address => 16#00010_1000#,
        Ucode_Id            => 2,
        Sspm                => Plcy_SSPM'(
                                Splm => 16#5#, -- L2
                                Ssecm => SSECM_SEC),
        Secret_Mask         => Plcy_Secret_Mask'(
                                Scp_Secret_Mask0 => 0,
                                Scp_Secret_Mask1 => 0,
                                Scp_Secret_Mask_lock0 => 0,
                                Scp_Secret_Mask_lock1 => 0),
        Debug_Control       => Default_Debug_Control,
        Mpu_Control         => Default_Mpu_Control,
        Device_Map_Group_0  => Default_Device_Map_Group_0,
        Device_Map_Group_1  => Default_Device_Map_Group_1,
        Device_Map_Group_2  => Default_Device_Map_Group_2,
        Device_Map_Group_3  => Default_Device_Map_Group_3,
        Core_Pmp            => RTOS_Core_Pmp,
        Io_Pmp              => Default_Io_Pmp
        );
-------------------------------------------------------------------------------------------------------------------------------
--***************************************************************************************************************************--
-------------------------------------------------------------------------------------------------------------------------------
    External_Policies : constant External_Policy_Array := External_Policy_Array'(
        Partition_ID_RTOS       => P_RTOS_Policy
    );

end Policies;
