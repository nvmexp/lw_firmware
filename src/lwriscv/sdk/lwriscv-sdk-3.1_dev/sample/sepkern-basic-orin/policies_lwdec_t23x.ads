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

package Policies
is

    Always_Clear_MPU_On_Switch : constant LwU1 := 1;

    P0_Debug_Control : constant Plcy_Debug_Control := Plcy_Debug_Control'(
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

    P0_Mpu_Control : constant Plcy_Mpu_Control := Plcy_Mpu_Control'(
        Start_Index       => 0,
        Entry_Count       => 128);

    P0_Device_Map_Group_0 : constant Plcy_Device_Map_Group_0 := (
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
            Readable => SUBMMODE_READ_DISABLE,
            Writable => SUBMMODE_WRITE_DISABLE,
            Reserved => 0));

    P0_Device_Map_Group_1 : constant Plcy_Device_Map_Group_1 := (
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
            Readable => SUBMMODE_READ_DISABLE,
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

    P0_Device_Map_Group_2 : constant Plcy_Device_Map_Group_2 := (
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

    P0_Device_Map_Group_3 : constant Plcy_Device_Map_Group_3 := (
        PLM => (
            Readable => SUBMMODE_READ_ENABLE,
            Writable => SUBMMODE_WRITE_ENABLE,
            Reserved => 0),
        HUB_DIO => (
            Readable => SUBMMODE_READ_ENABLE,
            Writable => SUBMMODE_WRITE_ENABLE,
            Reserved => 0),
        Reserved => 0);

    P0_Core_Pmp : constant Plcy_Core_Pmp := (
        0 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0010_1000#), 2)),
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        1 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0010_2000#), 2)),
                                Address_Mode => TOR,
                                Access_Mode => (
                                    Exelwtion_Access => PERMITTED,
                                    Write_Access => DENIED,
                                    Read_Access => PERMITTED,
                                    Reserved => 0),
                                Reserved => 0),

        2 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0010_3000#), 2)),
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        3 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0010_4000#), 2)),
                                Address_Mode => TOR,
                                Access_Mode => (
                                    Exelwtion_Access => PERMITTED,
                                    Write_Access => DENIED,
                                    Read_Access => PERMITTED,
                                    Reserved => 0),
                                Reserved => 0),

        4 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_1000#), 2)),
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        5 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_3000#), 2)),
                                Address_Mode => TOR,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => PERMITTED,
                                    Read_Access => PERMITTED,
                                    Reserved => 0),
                                Reserved => 0),

        6 => Core_Pmp_Entry'(   Addr => 16#0#,
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        7 => Core_Pmp_Entry'    (Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_3000#), 2)),
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        8 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_4000#), 2)),
                                Address_Mode => TOR,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => PERMITTED,
                                    Read_Access => PERMITTED,
                                    Reserved => 0),
                                Reserved => 0),

        9 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_f000#), 2)),
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        10 => Core_Pmp_Entry'(  Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0019_0000#), 2)),
                                Address_Mode => TOR,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => PERMITTED,
                                    Read_Access => PERMITTED,
                                    Reserved => 0),
                                Reserved => 0),

        11 => Core_Pmp_Entry'(  Addr => 16#0#,
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        12 => Core_Pmp_Entry'(  Addr => 16#0#,
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        13 => Core_Pmp_Entry'(  Addr => 16#0#,
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        14 => Core_Pmp_Entry'(  Addr => 16#0#,
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        15 => Core_Pmp_Entry'(  Addr => 16#0#,
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0));

    P0_Io_Pmp : constant Plcy_Io_Pmp := (
        0 => Io_Pmp_Entry'(
            Cfg  => Plcy_Io_Pmp_Cfg'(
                Read     => READ_ENABLE,
                Write    => WRITE_DISABLE,
                Fbdma    => ENABLE,
                Cpdma    => DISABLE,
                Sha      => DISABLE,
                Reserved => 0,
                PMB0     => ENABLE,
                PMB1     => DISABLE,
                PMB2     => DISABLE,
                PMB3     => DISABLE,
                PMB4     => DISABLE,
                PMB5     => DISABLE,
                PMB6     => DISABLE,
                PMB7     => DISABLE,
                Lock     => LOCK_UNLOCKED),
            Mode            => NAPOT,
            Addr_Lo_1k      => LW_PRGNLCL_RISCV_IOPMP_ADDR_LO_LO_1K_INIT,
            Addr_Lo_Above1k => 16#FFFF#,
            Addr_Hi         => 0),

        1 => Io_Pmp_Entry'(
            Cfg  => Plcy_Io_Pmp_Cfg'(
                Read     => READ_ENABLE,
                Write    => WRITE_DISABLE,
                Fbdma    => ENABLE,
                Cpdma    => DISABLE,
                Sha      => DISABLE,
                Reserved => 0,
                PMB0     => ENABLE,
                PMB1     => DISABLE,
                PMB2     => DISABLE,
                PMB3     => DISABLE,
                PMB4     => DISABLE,
                PMB5     => DISABLE,
                PMB6     => DISABLE,
                PMB7     => DISABLE,
                Lock     => LOCK_UNLOCKED),
            Mode            => NAPOT,
            Addr_Lo_1k      => LW_PRGNLCL_RISCV_IOPMP_ADDR_LO_LO_1K_INIT,
            Addr_Lo_Above1k => 16#FFFF#,
            Addr_Hi         => 0),

        2 => Io_Pmp_Entry'(
            Cfg  => Plcy_Io_Pmp_Cfg'(
                Read     => READ_ENABLE,
                Write    => WRITE_DISABLE,
                Fbdma    => ENABLE,
                Cpdma    => DISABLE,
                Sha      => DISABLE,
                Reserved => 0,
                PMB0     => ENABLE,
                PMB1     => DISABLE,
                PMB2     => DISABLE,
                PMB3     => DISABLE,
                PMB4     => DISABLE,
                PMB5     => DISABLE,
                PMB6     => DISABLE,
                PMB7     => DISABLE,
                Lock     => LOCK_UNLOCKED),
            Mode            => NAPOT,
            Addr_Lo_1k      => LW_PRGNLCL_RISCV_IOPMP_ADDR_LO_LO_1K_INIT,
            Addr_Lo_Above1k => 16#FFFF#,
            Addr_Hi         => 0),

        3 => Io_Pmp_Entry'(
            Cfg  => Plcy_Io_Pmp_Cfg'(
                Read     => READ_ENABLE,
                Write    => WRITE_DISABLE,
                Fbdma    => ENABLE,
                Cpdma    => DISABLE,
                Sha      => DISABLE,
                Reserved => 0,
                PMB0     => ENABLE,
                PMB1     => DISABLE,
                PMB2     => DISABLE,
                PMB3     => DISABLE,
                PMB4     => DISABLE,
                PMB5     => DISABLE,
                PMB6     => DISABLE,
                PMB7     => DISABLE,
                Lock     => LOCK_UNLOCKED),
            Mode            => NAPOT,
            Addr_Lo_1k      => LW_PRGNLCL_RISCV_IOPMP_ADDR_LO_LO_1K_INIT,
            Addr_Lo_Above1k => 16#FFFF#,
            Addr_Hi         => 0),

        4 => Io_Pmp_Entry'(
            Cfg  => Plcy_Io_Pmp_Cfg'(
                Read     => READ_ENABLE,
                Write    => WRITE_DISABLE,
                Fbdma    => ENABLE,
                Cpdma    => DISABLE,
                Sha      => DISABLE,
                Reserved => 0,
                PMB0     => ENABLE,
                PMB1     => DISABLE,
                PMB2     => DISABLE,
                PMB3     => DISABLE,
                PMB4     => DISABLE,
                PMB5     => DISABLE,
                PMB6     => DISABLE,
                PMB7     => DISABLE,
                Lock     => LOCK_UNLOCKED),
            Mode            => NAPOT,
            Addr_Lo_1k      => LW_PRGNLCL_RISCV_IOPMP_ADDR_LO_LO_1K_INIT,
            Addr_Lo_Above1k => 16#FFFF#,
            Addr_Hi         => 0),

        5 => Io_Pmp_Entry'(
            Cfg  => Plcy_Io_Pmp_Cfg'(
                Read     => READ_ENABLE,
                Write    => WRITE_DISABLE,
                Fbdma    => ENABLE,
                Cpdma    => DISABLE,
                Sha      => DISABLE,
                Reserved => 0,
                PMB0     => ENABLE,
                PMB1     => DISABLE,
                PMB2     => DISABLE,
                PMB3     => DISABLE,
                PMB4     => DISABLE,
                PMB5     => DISABLE,
                PMB6     => DISABLE,
                PMB7     => DISABLE,
                Lock     => LOCK_UNLOCKED),
            Mode            => NAPOT,
            Addr_Lo_1k      => LW_PRGNLCL_RISCV_IOPMP_ADDR_LO_LO_1K_INIT,
            Addr_Lo_Above1k => 16#FFFF#,
            Addr_Hi         => 0),

        6 => Io_Pmp_Entry'(
            Cfg  => Plcy_Io_Pmp_Cfg'(
                Read     => READ_ENABLE,
                Write    => WRITE_DISABLE,
                Fbdma    => ENABLE,
                Cpdma    => DISABLE,
                Sha      => DISABLE,
                Reserved => 0,
                PMB0     => ENABLE,
                PMB1     => DISABLE,
                PMB2     => DISABLE,
                PMB3     => DISABLE,
                PMB4     => DISABLE,
                PMB5     => DISABLE,
                PMB6     => DISABLE,
                PMB7     => DISABLE,
                Lock     => LOCK_UNLOCKED),
            Mode            => NAPOT,
            Addr_Lo_1k      => LW_PRGNLCL_RISCV_IOPMP_ADDR_LO_LO_1K_INIT,
            Addr_Lo_Above1k => 16#FFFF#,
            Addr_Hi         => 0),

        7 => Io_Pmp_Entry'(
            Cfg  => Plcy_Io_Pmp_Cfg'(
                Read     => READ_ENABLE,
                Write    => WRITE_DISABLE,
                Fbdma    => ENABLE,
                Cpdma    => DISABLE,
                Sha      => DISABLE,
                Reserved => 0,
                PMB0     => ENABLE,
                PMB1     => DISABLE,
                PMB2     => DISABLE,
                PMB3     => DISABLE,
                PMB4     => DISABLE,
                PMB5     => DISABLE,
                PMB6     => DISABLE,
                PMB7     => ENABLE,
                Lock     => LOCK_UNLOCKED),
            Mode            => NAPOT,
            Addr_Lo_1k      => LW_PRGNLCL_RISCV_IOPMP_ADDR_LO_LO_1K_INIT,
            Addr_Lo_Above1k => 16#FFFF#,
            Addr_Hi         => 0)
        );

    -------------------------------------------------------------------------------------------------------------------------------
    --***************************************************************************************************************************--
    -------------------------------------------------------------------------------------------------------------------------------
    -------------------------------------------------------------------------------------------------------------------------------
    --***************************************************************************************************************************--
    -------------------------------------------------------------------------------------------------------------------------------
    P1_Mpu_Control : constant Plcy_Mpu_Control := Plcy_Mpu_Control'(
        Start_Index       => 0,
        Entry_Count       => 128);

    P1_Core_Pmp : constant Plcy_Core_Pmp := (
        0 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0010_2000#), 2)),
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        1 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0010_3000#), 2)),
                                Address_Mode => TOR,
                                Access_Mode => (
                                    Exelwtion_Access => PERMITTED,
                                    Write_Access => DENIED,
                                    Read_Access => PERMITTED,
                                    Reserved => 0),
                                Reserved => 0),

        2 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0010_3000#), 2)),
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        3 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0010_4000#), 2)),
                                Address_Mode => TOR,
                                Access_Mode => (
                                    Exelwtion_Access => PERMITTED,
                                    Write_Access => DENIED,
                                    Read_Access => PERMITTED,
                                    Reserved => 0),
                                Reserved => 0),

        4 => Core_Pmp_Entry'(   Addr => 16#0#,
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        5 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_1000#), 2)),
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        6 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_3000#), 2)),
                                Address_Mode => TOR,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => PERMITTED,
                                    Read_Access => PERMITTED,
                                    Reserved => 0),
                                Reserved => 0),

        7 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_4000#), 2)),
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        8 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_5000#), 2)),
                                Address_Mode => TOR,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => PERMITTED,
                                    Read_Access => PERMITTED,
                                    Reserved => 0),
                                Reserved => 0),

        9 => Core_Pmp_Entry'(   Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0018_f000#), 2)),
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        10 => Core_Pmp_Entry'(  Addr => PMP_Address(Shift_Right(LwU64(16#0000_0000_0019_0000#), 2)),
                                Address_Mode => TOR,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => PERMITTED,
                                    Read_Access => PERMITTED,
                                    Reserved => 0),
                                Reserved => 0),

        11 => Core_Pmp_Entry'(  Addr => 16#0#,
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        12 => Core_Pmp_Entry'(  Addr => 16#0#,
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        13 => Core_Pmp_Entry'(  Addr => 16#0#,
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        14 => Core_Pmp_Entry'(  Addr => 16#0#,
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0),

        15 => Core_Pmp_Entry'(  Addr => 16#0#,
                                Address_Mode => OFF,
                                Access_Mode => (
                                    Exelwtion_Access => DENIED,
                                    Write_Access => DENIED,
                                    Read_Access => DENIED,
                                    Reserved => 0),
                                Reserved => 0));

    -------------------------------------------------------------------------------------------------------------------------------
    --***************************************************************************************************************************--
    -------------------------------------------------------------------------------------------------------------------------------
    -- Dummy policy var for testing FMC builds with more than 2 partitions/policies
    PX_Policy : constant External_Policy := External_Policy'(
                        Switchable_To       => Plcy_Switchable_To_Array'(others => PERMITTED),
                        Entry_Point_Address => 16#123123#,
                        Ucode_Id            => 4,
                        Sspm                => Plcy_SSPM'(
                                                Splm => 2#1#,
                                                Ssecm => SSECM_INSEC),
                        Secret_Mask         => Plcy_Secret_Mask'(
                                                Scp_Secret_Mask0 => 0,
                                                Scp_Secret_Mask1 => 0,
                                                Scp_Secret_Mask_lock0 => 0,
                                                Scp_Secret_Mask_lock1 => 0),
                        Debug_Control       => P0_Debug_Control,
                        Mpu_Control         => P1_Mpu_Control,
                        Device_Map_Group_0  => P0_Device_Map_Group_0,
                        Device_Map_Group_1  => P0_Device_Map_Group_1,
                        Device_Map_Group_2  => P0_Device_Map_Group_2,
                        Device_Map_Group_3  => P0_Device_Map_Group_3,
                        Core_Pmp            => P1_Core_Pmp,
                        SBI_Access_Config   => ALLOW_ALL_SBI,
                        Priv_Lockdown       => LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN_Register'(Lock => LOCK_UNLOCKED),
                        Io_Pmp              => P0_Io_Pmp);

    -------------------------------------------------------------------------------------------------------------------------------
    --***************************************************************************************************************************--
    -------------------------------------------------------------------------------------------------------------------------------
    External_Policies : constant External_Policy_Array := External_Policy_Array'(
           0 => External_Policy'(
                    Switchable_To       => Plcy_Switchable_To_Array'(others => PERMITTED),
                    Entry_Point_Address => 16#123123#,
                    Ucode_Id            => 4,
                    Sspm                => Plcy_SSPM'(
                                            Splm => 2#1#,
                                            Ssecm => SSECM_INSEC),
                    Secret_Mask         => Plcy_Secret_Mask'(
                                            Scp_Secret_Mask0 => 0,
                                            Scp_Secret_Mask1 => 0,
                                            Scp_Secret_Mask_lock0 => 0,
                                            Scp_Secret_Mask_lock1 => 0),
                    Debug_Control       => P0_Debug_Control,
                    Mpu_Control         => P0_Mpu_Control,
                    Device_Map_Group_0  => P0_Device_Map_Group_0,
                    Device_Map_Group_1  => P0_Device_Map_Group_1,
                    Device_Map_Group_2  => P0_Device_Map_Group_2,
                    Device_Map_Group_3  => P0_Device_Map_Group_3,
                    Core_Pmp            => P0_Core_Pmp,
                    SBI_Access_Config   => ALLOW_ALL_SBI,
                    Priv_Lockdown       => LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN_Register'(Lock => LOCK_UNLOCKED),
                    Io_Pmp              => P0_Io_Pmp)
#if PARTITION_COUNT >= 2 then
,
           1 => External_Policy'(
                    Switchable_To       => Plcy_Switchable_To_Array'(
                                                 0 => PERMITTED,
                                                 others => DENIED),
                    Entry_Point_Address => 16#123123#,
                    Ucode_Id            => 4,
                    Sspm                => Plcy_SSPM'(
                                            Splm => 2#1#,
                                            Ssecm => SSECM_INSEC),
                    Secret_Mask         => Plcy_Secret_Mask'(
                                            Scp_Secret_Mask0 => 0,
                                            Scp_Secret_Mask1 => 0,
                                            Scp_Secret_Mask_lock0 => 0,
                                            Scp_Secret_Mask_lock1 => 0),
                    Debug_Control       => P0_Debug_Control,
                    Mpu_Control         => P1_Mpu_Control,
                    Device_Map_Group_0  => P0_Device_Map_Group_0,
                    Device_Map_Group_1  => P0_Device_Map_Group_1,
                    Device_Map_Group_2  => P0_Device_Map_Group_2,
                    Device_Map_Group_3  => P0_Device_Map_Group_3,
                    Core_Pmp            => P1_Core_Pmp,
                    SBI_Access_Config   => ALLOW_ALL_SBI,
                    Priv_Lockdown       => LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN_Register'(Lock => LOCK_UNLOCKED),
                    Io_Pmp              => P0_Io_Pmp)
#end if;
#if PARTITION_COUNT >= 3 then
,
           2 => PX_Policy
#end if;
#if PARTITION_COUNT >= 4 then
,
           3 => PX_Policy
#end if;
#if PARTITION_COUNT >= 5 then
,
           4 => PX_Policy
#end if;
#if PARTITION_COUNT >= 6 then
,
           5 => PX_Policy
#end if;
#if PARTITION_COUNT >= 7 then
,
           6 => PX_Policy
#end if;
#if PARTITION_COUNT >= 8 then
,
           7 => PX_Policy
#end if;
   );

end Policies;
