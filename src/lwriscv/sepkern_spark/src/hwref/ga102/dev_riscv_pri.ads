-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;

--******************************  DEV_RISCV_PRI  ******************************--

package Dev_Riscv_Pri
with SPARK_MODE => On
is

   LW_FALCON2_GSP_BASE : constant := 16#0011_1000#;
   LW_FALCON2_PWR_BASE : constant := 16#0010_b000#;
   LW_FALCON2_SEC_BASE : constant := 16#0084_1000#;

---------- Register Declaration ----------

   Lw_Priscv_Riscv_Exe_Priv_Level_Mask    : constant Bar0_Addr := 16#0000_0304#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Priscv_Riscv_Exe_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_EXE_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 3;
        F_Write_Protection at 0 range 4 .. 7;
        F_Read_Violation at 0 range 8 .. 8;
        F_Write_Violation at 0 range 9 .. 9;
        F_Source_Read_Control at 0 range 10 .. 10;
        F_Source_Write_Control at 0 range 11 .. 11;
        F_Source_Enable at 0 range 12 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask    : constant Bar0_Addr := 16#0000_0308#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Priscv_Riscv_Cpuctl_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 3;
        F_Write_Protection at 0 range 4 .. 7;
        F_Read_Violation at 0 range 8 .. 8;
        F_Write_Violation at 0 range 9 .. 9;
        F_Source_Read_Control at 0 range 10 .. 10;
        F_Source_Write_Control at 0 range 11 .. 11;
        F_Source_Enable at 0 range 12 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Irq_Priv_Level_Mask    : constant Bar0_Addr := 16#0000_0300#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Priscv_Riscv_Irq_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IRQ_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 3;
        F_Write_Protection at 0 range 4 .. 7;
        F_Read_Violation at 0 range 8 .. 8;
        F_Write_Violation at 0 range 9 .. 9;
        F_Source_Read_Control at 0 range 10 .. 10;
        F_Source_Write_Control at 0 range 11 .. 11;
        F_Source_Enable at 0 range 12 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask    : constant Bar0_Addr := 16#0000_0314#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Priscv_Riscv_Dbgctl_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_DBGCTL_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 3;
        F_Write_Protection at 0 range 4 .. 7;
        F_Read_Violation at 0 range 8 .. 8;
        F_Write_Violation at 0 range 9 .. 9;
        F_Source_Read_Control at 0 range 10 .. 10;
        F_Source_Write_Control at 0 range 11 .. 11;
        F_Source_Enable at 0 range 12 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask    : constant Bar0_Addr := 16#0000_0310#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Priscv_Riscv_Bootvec_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BOOTVEC_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 3;
        F_Write_Protection at 0 range 4 .. 7;
        F_Read_Violation at 0 range 8 .. 8;
        F_Write_Violation at 0 range 9 .. 9;
        F_Source_Read_Control at 0 range 10 .. 10;
        F_Source_Write_Control at 0 range 11 .. 11;
        F_Source_Enable at 0 range 12 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Bcr_Priv_Level_Mask    : constant Bar0_Addr := 16#0000_0664#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Priscv_Riscv_Bcr_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 3;
        F_Write_Protection at 0 range 4 .. 7;
        F_Read_Violation at 0 range 8 .. 8;
        F_Write_Violation at 0 range 9 .. 9;
        F_Source_Read_Control at 0 range 10 .. 10;
        F_Source_Write_Control at 0 range 11 .. 11;
        F_Source_Enable at 0 range 12 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask    : constant Bar0_Addr := 16#0000_0690#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 3;
        F_Write_Protection at 0 range 4 .. 7;
        F_Read_Violation at 0 range 8 .. 8;
        F_Write_Violation at 0 range 9 .. 9;
        F_Source_Read_Control at 0 range 10 .. 10;
        F_Source_Write_Control at 0 range 11 .. 11;
        F_Source_Enable at 0 range 12 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask    : constant Bar0_Addr := 16#0000_030C#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Priscv_Riscv_Lwconfig_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_LWCONFIG_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 3;
        F_Write_Protection at 0 range 4 .. 7;
        F_Read_Violation at 0 range 8 .. 8;
        F_Write_Violation at 0 range 9 .. 9;
        F_Source_Read_Control at 0 range 10 .. 10;
        F_Source_Write_Control at 0 range 11 .. 11;
        F_Source_Enable at 0 range 12 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask    : constant Bar0_Addr := 16#0000_0318#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Priscv_Riscv_Tracebuf_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_TRACEBUF_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 3;
        F_Write_Protection at 0 range 4 .. 7;
        F_Read_Violation at 0 range 8 .. 8;
        F_Write_Violation at 0 range 9 .. 9;
        F_Source_Read_Control at 0 range 10 .. 10;
        F_Source_Write_Control at 0 range 11 .. 11;
        F_Source_Enable at 0 range 12 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Msip_Priv_Level_Mask    : constant Bar0_Addr := 16#0000_031C#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Priscv_Riscv_Msip_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_MSIP_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 3;
        F_Write_Protection at 0 range 4 .. 7;
        F_Read_Violation at 0 range 8 .. 8;
        F_Write_Violation at 0 range 9 .. 9;
        F_Source_Read_Control at 0 range 10 .. 10;
        F_Source_Write_Control at 0 range 11 .. 11;
        F_Source_Enable at 0 range 12 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Irqmset    : constant Bar0_Addr := 16#0000_0520#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMSET_GPTMR_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmset_Gptmr_Set :
        constant LW_PRISCV_RISCV_IRQMSET_GPTMR_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMSET_WDTMR_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmset_Wdtmr_Set :
        constant LW_PRISCV_RISCV_IRQMSET_WDTMR_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMSET_MTHD_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmset_Mthd_Set :
        constant LW_PRISCV_RISCV_IRQMSET_MTHD_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMSET_CTXSW_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmset_Ctxsw_Set :
        constant LW_PRISCV_RISCV_IRQMSET_CTXSW_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMSET_HALT_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmset_Halt_Set :
        constant LW_PRISCV_RISCV_IRQMSET_HALT_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMSET_EXTERR_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmset_Exterr_Set :
        constant LW_PRISCV_RISCV_IRQMSET_EXTERR_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMSET_SWGEN0_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmset_Swgen0_Set :
        constant LW_PRISCV_RISCV_IRQMSET_SWGEN0_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMSET_SWGEN1_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmset_Swgen1_Set :
        constant LW_PRISCV_RISCV_IRQMSET_SWGEN1_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMSET_EXT_FIELD is new LwU8;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMSET_DMA_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmset_Dma_Set :
        constant LW_PRISCV_RISCV_IRQMSET_DMA_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMSET_SHA_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmset_Sha_Set :
        constant LW_PRISCV_RISCV_IRQMSET_SHA_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMSET_MEMERR_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmset_Memerr_Set :
        constant LW_PRISCV_RISCV_IRQMSET_MEMERR_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMSET_ICD_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmset_Icd_Set :
        constant LW_PRISCV_RISCV_IRQMSET_ICD_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMSET_IOPMP_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmset_Iopmp_Set :
        constant LW_PRISCV_RISCV_IRQMSET_IOPMP_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMSET_CORE_MISMATCH_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmset_Core_Mismatch_Set :
        constant LW_PRISCV_RISCV_IRQMSET_CORE_MISMATCH_FIELD
        := 16#0000_0001#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IRQMSET_REGISTER is
    record
        F_Gptmr    : LW_PRISCV_RISCV_IRQMSET_GPTMR_FIELD;
        F_Wdtmr    : LW_PRISCV_RISCV_IRQMSET_WDTMR_FIELD;
        F_Mthd    : LW_PRISCV_RISCV_IRQMSET_MTHD_FIELD;
        F_Ctxsw    : LW_PRISCV_RISCV_IRQMSET_CTXSW_FIELD;
        F_Halt    : LW_PRISCV_RISCV_IRQMSET_HALT_FIELD;
        F_Exterr    : LW_PRISCV_RISCV_IRQMSET_EXTERR_FIELD;
        F_Swgen0    : LW_PRISCV_RISCV_IRQMSET_SWGEN0_FIELD;
        F_Swgen1    : LW_PRISCV_RISCV_IRQMSET_SWGEN1_FIELD;
        F_Ext    : LW_PRISCV_RISCV_IRQMSET_EXT_FIELD;
        F_Dma    : LW_PRISCV_RISCV_IRQMSET_DMA_FIELD;
        F_Sha    : LW_PRISCV_RISCV_IRQMSET_SHA_FIELD;
        F_Memerr    : LW_PRISCV_RISCV_IRQMSET_MEMERR_FIELD;
        F_Icd    : LW_PRISCV_RISCV_IRQMSET_ICD_FIELD;
        F_Iopmp    : LW_PRISCV_RISCV_IRQMSET_IOPMP_FIELD;
        F_Core_Mismatch    : LW_PRISCV_RISCV_IRQMSET_CORE_MISMATCH_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IRQMSET_REGISTER use
    record
        F_Gptmr at 0 range 0 .. 0;
        F_Wdtmr at 0 range 1 .. 1;
        F_Mthd at 0 range 2 .. 2;
        F_Ctxsw at 0 range 3 .. 3;
        F_Halt at 0 range 4 .. 4;
        F_Exterr at 0 range 5 .. 5;
        F_Swgen0 at 0 range 6 .. 6;
        F_Swgen1 at 0 range 7 .. 7;
        F_Ext at 0 range 8 .. 15;
        F_Dma at 0 range 16 .. 16;
        F_Sha at 0 range 17 .. 17;
        F_Memerr at 0 range 18 .. 18;
        F_Icd at 0 range 22 .. 22;
        F_Iopmp at 0 range 23 .. 23;
        F_Core_Mismatch at 0 range 24 .. 24;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Irqmclr    : constant Bar0_Addr := 16#0000_0524#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMCLR_GPTMR_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmclr_Gptmr_Set :
        constant LW_PRISCV_RISCV_IRQMCLR_GPTMR_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMCLR_WDTMR_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmclr_Wdtmr_Set :
        constant LW_PRISCV_RISCV_IRQMCLR_WDTMR_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMCLR_MTHD_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmclr_Mthd_Set :
        constant LW_PRISCV_RISCV_IRQMCLR_MTHD_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMCLR_CTXSW_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmclr_Ctxsw_Set :
        constant LW_PRISCV_RISCV_IRQMCLR_CTXSW_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMCLR_HALT_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmclr_Halt_Set :
        constant LW_PRISCV_RISCV_IRQMCLR_HALT_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMCLR_EXTERR_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmclr_Exterr_Set :
        constant LW_PRISCV_RISCV_IRQMCLR_EXTERR_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMCLR_SWGEN0_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmclr_Swgen0_Set :
        constant LW_PRISCV_RISCV_IRQMCLR_SWGEN0_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMCLR_SWGEN1_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmclr_Swgen1_Set :
        constant LW_PRISCV_RISCV_IRQMCLR_SWGEN1_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMCLR_EXT_FIELD is new LwU8;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMCLR_DMA_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmclr_Dma_Set :
        constant LW_PRISCV_RISCV_IRQMCLR_DMA_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMCLR_SHA_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmclr_Sha_Set :
        constant LW_PRISCV_RISCV_IRQMCLR_SHA_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMCLR_MEMERR_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmclr_Memerr_Set :
        constant LW_PRISCV_RISCV_IRQMCLR_MEMERR_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMCLR_ICD_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmclr_Icd_Set :
        constant LW_PRISCV_RISCV_IRQMCLR_ICD_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMCLR_IOPMP_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmclr_Iopmp_Set :
        constant LW_PRISCV_RISCV_IRQMCLR_IOPMP_FIELD
        := 16#0000_0001#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IRQMCLR_REGISTER is
    record
        F_Gptmr    : LW_PRISCV_RISCV_IRQMCLR_GPTMR_FIELD;
        F_Wdtmr    : LW_PRISCV_RISCV_IRQMCLR_WDTMR_FIELD;
        F_Mthd    : LW_PRISCV_RISCV_IRQMCLR_MTHD_FIELD;
        F_Ctxsw    : LW_PRISCV_RISCV_IRQMCLR_CTXSW_FIELD;
        F_Halt    : LW_PRISCV_RISCV_IRQMCLR_HALT_FIELD;
        F_Exterr    : LW_PRISCV_RISCV_IRQMCLR_EXTERR_FIELD;
        F_Swgen0    : LW_PRISCV_RISCV_IRQMCLR_SWGEN0_FIELD;
        F_Swgen1    : LW_PRISCV_RISCV_IRQMCLR_SWGEN1_FIELD;
        F_Ext    : LW_PRISCV_RISCV_IRQMCLR_EXT_FIELD;
        F_Dma    : LW_PRISCV_RISCV_IRQMCLR_DMA_FIELD;
        F_Sha    : LW_PRISCV_RISCV_IRQMCLR_SHA_FIELD;
        F_Memerr    : LW_PRISCV_RISCV_IRQMCLR_MEMERR_FIELD;
        F_Icd    : LW_PRISCV_RISCV_IRQMCLR_ICD_FIELD;
        F_Iopmp    : LW_PRISCV_RISCV_IRQMCLR_IOPMP_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IRQMCLR_REGISTER use
    record
        F_Gptmr at 0 range 0 .. 0;
        F_Wdtmr at 0 range 1 .. 1;
        F_Mthd at 0 range 2 .. 2;
        F_Ctxsw at 0 range 3 .. 3;
        F_Halt at 0 range 4 .. 4;
        F_Exterr at 0 range 5 .. 5;
        F_Swgen0 at 0 range 6 .. 6;
        F_Swgen1 at 0 range 7 .. 7;
        F_Ext at 0 range 8 .. 15;
        F_Dma at 0 range 16 .. 16;
        F_Sha at 0 range 17 .. 17;
        F_Memerr at 0 range 18 .. 18;
        F_Icd at 0 range 22 .. 22;
        F_Iopmp at 0 range 23 .. 23;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Irqmask    : constant Bar0_Addr := 16#0000_0528#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_GPTMR_FIELD is
    (
        Lw_Priscv_Riscv_Irqmask_Gptmr_Unset,
        Lw_Priscv_Riscv_Irqmask_Gptmr_Set
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQMASK_GPTMR_FIELD use
    (
        Lw_Priscv_Riscv_Irqmask_Gptmr_Unset => 0,
        Lw_Priscv_Riscv_Irqmask_Gptmr_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_WDTMR_FIELD is
    (
        Lw_Priscv_Riscv_Irqmask_Wdtmr_Unset,
        Lw_Priscv_Riscv_Irqmask_Wdtmr_Set
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQMASK_WDTMR_FIELD use
    (
        Lw_Priscv_Riscv_Irqmask_Wdtmr_Unset => 0,
        Lw_Priscv_Riscv_Irqmask_Wdtmr_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_MTHD_FIELD is
    (
        Lw_Priscv_Riscv_Irqmask_Mthd_Unset,
        Lw_Priscv_Riscv_Irqmask_Mthd_Set
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQMASK_MTHD_FIELD use
    (
        Lw_Priscv_Riscv_Irqmask_Mthd_Unset => 0,
        Lw_Priscv_Riscv_Irqmask_Mthd_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_CTXSW_FIELD is
    (
        Lw_Priscv_Riscv_Irqmask_Ctxsw_Unset,
        Lw_Priscv_Riscv_Irqmask_Ctxsw_Set
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQMASK_CTXSW_FIELD use
    (
        Lw_Priscv_Riscv_Irqmask_Ctxsw_Unset => 0,
        Lw_Priscv_Riscv_Irqmask_Ctxsw_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_HALT_FIELD is
    (
        Lw_Priscv_Riscv_Irqmask_Halt_Unset,
        Lw_Priscv_Riscv_Irqmask_Halt_Set
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQMASK_HALT_FIELD use
    (
        Lw_Priscv_Riscv_Irqmask_Halt_Unset => 0,
        Lw_Priscv_Riscv_Irqmask_Halt_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_EXTERR_FIELD is
    (
        Lw_Priscv_Riscv_Irqmask_Exterr_Unset,
        Lw_Priscv_Riscv_Irqmask_Exterr_Set
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQMASK_EXTERR_FIELD use
    (
        Lw_Priscv_Riscv_Irqmask_Exterr_Unset => 0,
        Lw_Priscv_Riscv_Irqmask_Exterr_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_SWGEN0_FIELD is
    (
        Lw_Priscv_Riscv_Irqmask_Swgen0_Unset,
        Lw_Priscv_Riscv_Irqmask_Swgen0_Set
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQMASK_SWGEN0_FIELD use
    (
        Lw_Priscv_Riscv_Irqmask_Swgen0_Unset => 0,
        Lw_Priscv_Riscv_Irqmask_Swgen0_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_SWGEN1_FIELD is
    (
        Lw_Priscv_Riscv_Irqmask_Swgen1_Unset,
        Lw_Priscv_Riscv_Irqmask_Swgen1_Set
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQMASK_SWGEN1_FIELD use
    (
        Lw_Priscv_Riscv_Irqmask_Swgen1_Unset => 0,
        Lw_Priscv_Riscv_Irqmask_Swgen1_Set => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ1_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmask_Ext_Extirq1_Unset :
        constant LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ1_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ2_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmask_Ext_Extirq2_Unset :
        constant LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ2_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ3_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmask_Ext_Extirq3_Unset :
        constant LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ3_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ4_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmask_Ext_Extirq4_Unset :
        constant LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ4_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ5_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmask_Ext_Extirq5_Unset :
        constant LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ5_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ6_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmask_Ext_Extirq6_Unset :
        constant LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ6_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ7_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmask_Ext_Extirq7_Unset :
        constant LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ7_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ8_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irqmask_Ext_Extirq8_Unset :
        constant LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ8_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_DMA_FIELD is
    (
        Lw_Priscv_Riscv_Irqmask_Dma_Unset,
        Lw_Priscv_Riscv_Irqmask_Dma_Set
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQMASK_DMA_FIELD use
    (
        Lw_Priscv_Riscv_Irqmask_Dma_Unset => 0,
        Lw_Priscv_Riscv_Irqmask_Dma_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_SHA_FIELD is
    (
        Lw_Priscv_Riscv_Irqmask_Sha_Unset,
        Lw_Priscv_Riscv_Irqmask_Sha_Set
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQMASK_SHA_FIELD use
    (
        Lw_Priscv_Riscv_Irqmask_Sha_Unset => 0,
        Lw_Priscv_Riscv_Irqmask_Sha_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_MEMERR_FIELD is
    (
        Lw_Priscv_Riscv_Irqmask_Memerr_Unset,
        Lw_Priscv_Riscv_Irqmask_Memerr_Set
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQMASK_MEMERR_FIELD use
    (
        Lw_Priscv_Riscv_Irqmask_Memerr_Unset => 0,
        Lw_Priscv_Riscv_Irqmask_Memerr_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_ICD_FIELD is
    (
        Lw_Priscv_Riscv_Irqmask_Icd_Unset,
        Lw_Priscv_Riscv_Irqmask_Icd_Set
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQMASK_ICD_FIELD use
    (
        Lw_Priscv_Riscv_Irqmask_Icd_Unset => 0,
        Lw_Priscv_Riscv_Irqmask_Icd_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_IOPMP_FIELD is
    (
        Lw_Priscv_Riscv_Irqmask_Iopmp_Unset,
        Lw_Priscv_Riscv_Irqmask_Iopmp_Set
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQMASK_IOPMP_FIELD use
    (
        Lw_Priscv_Riscv_Irqmask_Iopmp_Unset => 0,
        Lw_Priscv_Riscv_Irqmask_Iopmp_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_CORE_MISMATCH_FIELD is
    (
        Lw_Priscv_Riscv_Irqmask_Core_Mismatch_Disable,
        Lw_Priscv_Riscv_Irqmask_Core_Mismatch_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQMASK_CORE_MISMATCH_FIELD use
    (
        Lw_Priscv_Riscv_Irqmask_Core_Mismatch_Disable => 0,
        Lw_Priscv_Riscv_Irqmask_Core_Mismatch_Enable => 1
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IRQMASK_REGISTER is
    record
        F_Gptmr    : LW_PRISCV_RISCV_IRQMASK_GPTMR_FIELD;
        F_Wdtmr    : LW_PRISCV_RISCV_IRQMASK_WDTMR_FIELD;
        F_Mthd    : LW_PRISCV_RISCV_IRQMASK_MTHD_FIELD;
        F_Ctxsw    : LW_PRISCV_RISCV_IRQMASK_CTXSW_FIELD;
        F_Halt    : LW_PRISCV_RISCV_IRQMASK_HALT_FIELD;
        F_Exterr    : LW_PRISCV_RISCV_IRQMASK_EXTERR_FIELD;
        F_Swgen0    : LW_PRISCV_RISCV_IRQMASK_SWGEN0_FIELD;
        F_Swgen1    : LW_PRISCV_RISCV_IRQMASK_SWGEN1_FIELD;
        F_Ext_Extirq1    : LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ1_FIELD;
        F_Ext_Extirq2    : LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ2_FIELD;
        F_Ext_Extirq3    : LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ3_FIELD;
        F_Ext_Extirq4    : LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ4_FIELD;
        F_Ext_Extirq5    : LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ5_FIELD;
        F_Ext_Extirq6    : LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ6_FIELD;
        F_Ext_Extirq7    : LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ7_FIELD;
        F_Ext_Extirq8    : LW_PRISCV_RISCV_IRQMASK_EXT_EXTIRQ8_FIELD;
        F_Dma    : LW_PRISCV_RISCV_IRQMASK_DMA_FIELD;
        F_Sha    : LW_PRISCV_RISCV_IRQMASK_SHA_FIELD;
        F_Memerr    : LW_PRISCV_RISCV_IRQMASK_MEMERR_FIELD;
        F_Icd    : LW_PRISCV_RISCV_IRQMASK_ICD_FIELD;
        F_Iopmp    : LW_PRISCV_RISCV_IRQMASK_IOPMP_FIELD;
        F_Core_Mismatch    : LW_PRISCV_RISCV_IRQMASK_CORE_MISMATCH_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IRQMASK_REGISTER use
    record
        F_Gptmr at 0 range 0 .. 0;
        F_Wdtmr at 0 range 1 .. 1;
        F_Mthd at 0 range 2 .. 2;
        F_Ctxsw at 0 range 3 .. 3;
        F_Halt at 0 range 4 .. 4;
        F_Exterr at 0 range 5 .. 5;
        F_Swgen0 at 0 range 6 .. 6;
        F_Swgen1 at 0 range 7 .. 7;
        F_Ext_Extirq1 at 0 range 8 .. 8;
        F_Ext_Extirq2 at 0 range 9 .. 9;
        F_Ext_Extirq3 at 0 range 10 .. 10;
        F_Ext_Extirq4 at 0 range 11 .. 11;
        F_Ext_Extirq5 at 0 range 12 .. 12;
        F_Ext_Extirq6 at 0 range 13 .. 13;
        F_Ext_Extirq7 at 0 range 14 .. 14;
        F_Ext_Extirq8 at 0 range 15 .. 15;
        F_Dma at 0 range 16 .. 16;
        F_Sha at 0 range 17 .. 17;
        F_Memerr at 0 range 18 .. 18;
        F_Icd at 0 range 22 .. 22;
        F_Iopmp at 0 range 23 .. 23;
        F_Core_Mismatch at 0 range 24 .. 24;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Irqdest    : constant Bar0_Addr := 16#0000_052C#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_GPTMR_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Gptmr_Riscv,
        Lw_Priscv_Riscv_Irqdest_Gptmr_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_GPTMR_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Gptmr_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Gptmr_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_WDTMR_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Wdtmr_Riscv,
        Lw_Priscv_Riscv_Irqdest_Wdtmr_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_WDTMR_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Wdtmr_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Wdtmr_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_MTHD_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Mthd_Riscv,
        Lw_Priscv_Riscv_Irqdest_Mthd_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_MTHD_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Mthd_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Mthd_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_CTXSW_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Ctxsw_Riscv,
        Lw_Priscv_Riscv_Irqdest_Ctxsw_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_CTXSW_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Ctxsw_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Ctxsw_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_HALT_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Halt_Riscv,
        Lw_Priscv_Riscv_Irqdest_Halt_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_HALT_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Halt_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Halt_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_EXTERR_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Exterr_Riscv,
        Lw_Priscv_Riscv_Irqdest_Exterr_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_EXTERR_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Exterr_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Exterr_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_SWGEN0_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Swgen0_Riscv,
        Lw_Priscv_Riscv_Irqdest_Swgen0_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_SWGEN0_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Swgen0_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Swgen0_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_SWGEN1_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Swgen1_Riscv,
        Lw_Priscv_Riscv_Irqdest_Swgen1_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_SWGEN1_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Swgen1_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Swgen1_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ1_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq1_Riscv,
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq1_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ1_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq1_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq1_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ2_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq2_Riscv,
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq2_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ2_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq2_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq2_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ3_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq3_Riscv,
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq3_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ3_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq3_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq3_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ4_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq4_Riscv,
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq4_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ4_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq4_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq4_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ5_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq5_Riscv,
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq5_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ5_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq5_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq5_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ6_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq6_Riscv,
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq6_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ6_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq6_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq6_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ7_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq7_Riscv,
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq7_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ7_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq7_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq7_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ8_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq8_Riscv,
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq8_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ8_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq8_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Ext_Extirq8_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_DMA_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Dma_Riscv,
        Lw_Priscv_Riscv_Irqdest_Dma_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_DMA_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Dma_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Dma_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_SHA_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Sha_Riscv,
        Lw_Priscv_Riscv_Irqdest_Sha_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_SHA_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Sha_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Sha_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_MEMERR_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Memerr_Riscv,
        Lw_Priscv_Riscv_Irqdest_Memerr_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_MEMERR_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Memerr_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Memerr_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_ICD_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Icd_Riscv,
        Lw_Priscv_Riscv_Irqdest_Icd_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_ICD_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Icd_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Icd_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_IOPMP_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Iopmp_Riscv,
        Lw_Priscv_Riscv_Irqdest_Iopmp_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_IOPMP_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Iopmp_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Iopmp_Host => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_CORE_MISMATCH_FIELD is
    (
        Lw_Priscv_Riscv_Irqdest_Core_Mismatch_Riscv,
        Lw_Priscv_Riscv_Irqdest_Core_Mismatch_Host
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDEST_CORE_MISMATCH_FIELD use
    (
        Lw_Priscv_Riscv_Irqdest_Core_Mismatch_Riscv => 0,
        Lw_Priscv_Riscv_Irqdest_Core_Mismatch_Host => 1
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IRQDEST_REGISTER is
    record
        F_Gptmr    : LW_PRISCV_RISCV_IRQDEST_GPTMR_FIELD;
        F_Wdtmr    : LW_PRISCV_RISCV_IRQDEST_WDTMR_FIELD;
        F_Mthd    : LW_PRISCV_RISCV_IRQDEST_MTHD_FIELD;
        F_Ctxsw    : LW_PRISCV_RISCV_IRQDEST_CTXSW_FIELD;
        F_Halt    : LW_PRISCV_RISCV_IRQDEST_HALT_FIELD;
        F_Exterr    : LW_PRISCV_RISCV_IRQDEST_EXTERR_FIELD;
        F_Swgen0    : LW_PRISCV_RISCV_IRQDEST_SWGEN0_FIELD;
        F_Swgen1    : LW_PRISCV_RISCV_IRQDEST_SWGEN1_FIELD;
        F_Ext_Extirq1    : LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ1_FIELD;
        F_Ext_Extirq2    : LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ2_FIELD;
        F_Ext_Extirq3    : LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ3_FIELD;
        F_Ext_Extirq4    : LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ4_FIELD;
        F_Ext_Extirq5    : LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ5_FIELD;
        F_Ext_Extirq6    : LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ6_FIELD;
        F_Ext_Extirq7    : LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ7_FIELD;
        F_Ext_Extirq8    : LW_PRISCV_RISCV_IRQDEST_EXT_EXTIRQ8_FIELD;
        F_Dma    : LW_PRISCV_RISCV_IRQDEST_DMA_FIELD;
        F_Sha    : LW_PRISCV_RISCV_IRQDEST_SHA_FIELD;
        F_Memerr    : LW_PRISCV_RISCV_IRQDEST_MEMERR_FIELD;
        F_Icd    : LW_PRISCV_RISCV_IRQDEST_ICD_FIELD;
        F_Iopmp    : LW_PRISCV_RISCV_IRQDEST_IOPMP_FIELD;
        F_Core_Mismatch    : LW_PRISCV_RISCV_IRQDEST_CORE_MISMATCH_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IRQDEST_REGISTER use
    record
        F_Gptmr at 0 range 0 .. 0;
        F_Wdtmr at 0 range 1 .. 1;
        F_Mthd at 0 range 2 .. 2;
        F_Ctxsw at 0 range 3 .. 3;
        F_Halt at 0 range 4 .. 4;
        F_Exterr at 0 range 5 .. 5;
        F_Swgen0 at 0 range 6 .. 6;
        F_Swgen1 at 0 range 7 .. 7;
        F_Ext_Extirq1 at 0 range 8 .. 8;
        F_Ext_Extirq2 at 0 range 9 .. 9;
        F_Ext_Extirq3 at 0 range 10 .. 10;
        F_Ext_Extirq4 at 0 range 11 .. 11;
        F_Ext_Extirq5 at 0 range 12 .. 12;
        F_Ext_Extirq6 at 0 range 13 .. 13;
        F_Ext_Extirq7 at 0 range 14 .. 14;
        F_Ext_Extirq8 at 0 range 15 .. 15;
        F_Dma at 0 range 16 .. 16;
        F_Sha at 0 range 17 .. 17;
        F_Memerr at 0 range 18 .. 18;
        F_Icd at 0 range 22 .. 22;
        F_Iopmp at 0 range 23 .. 23;
        F_Core_Mismatch at 0 range 24 .. 24;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Irqtype    : constant Bar0_Addr := 16#0000_0530#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_GPTMR_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Gptmr_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Gptmr_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_GPTMR_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Gptmr_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Gptmr_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_WDTMR_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Wdtmr_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Wdtmr_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_WDTMR_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Wdtmr_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Wdtmr_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_MTHD_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Mthd_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Mthd_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_MTHD_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Mthd_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Mthd_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_CTXSW_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Ctxsw_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Ctxsw_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_CTXSW_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Ctxsw_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Ctxsw_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_HALT_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Halt_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Halt_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_HALT_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Halt_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Halt_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_EXTERR_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Exterr_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Exterr_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_EXTERR_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Exterr_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Exterr_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_SWGEN0_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Swgen0_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Swgen0_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_SWGEN0_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Swgen0_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Swgen0_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_SWGEN1_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Swgen1_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Swgen1_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_SWGEN1_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Swgen1_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Swgen1_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ1_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq1_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq1_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ1_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq1_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq1_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ2_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq2_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq2_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ2_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq2_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq2_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ3_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq3_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq3_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ3_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq3_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq3_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ4_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq4_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq4_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ4_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq4_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq4_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ5_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq5_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq5_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ5_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq5_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq5_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ6_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq6_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq6_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ6_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq6_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq6_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ7_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq7_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq7_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ7_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq7_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq7_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ8_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq8_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq8_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ8_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq8_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Ext_Extirq8_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_DMA_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Dma_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Dma_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_DMA_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Dma_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Dma_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_SHA_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Sha_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Sha_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_SHA_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Sha_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Sha_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_MEMERR_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Memerr_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Memerr_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_MEMERR_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Memerr_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Memerr_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_ICD_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Icd_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Icd_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_ICD_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Icd_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Icd_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_IOPMP_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Iopmp_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Iopmp_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_IOPMP_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Iopmp_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Iopmp_Host_Nonstall => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_CORE_MISMATCH_FIELD is
    (
        Lw_Priscv_Riscv_Irqtype_Core_Mismatch_Host_Normal,
        Lw_Priscv_Riscv_Irqtype_Core_Mismatch_Host_Nonstall
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQTYPE_CORE_MISMATCH_FIELD use
    (
        Lw_Priscv_Riscv_Irqtype_Core_Mismatch_Host_Normal => 0,
        Lw_Priscv_Riscv_Irqtype_Core_Mismatch_Host_Nonstall => 1
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IRQTYPE_REGISTER is
    record
        F_Gptmr    : LW_PRISCV_RISCV_IRQTYPE_GPTMR_FIELD;
        F_Wdtmr    : LW_PRISCV_RISCV_IRQTYPE_WDTMR_FIELD;
        F_Mthd    : LW_PRISCV_RISCV_IRQTYPE_MTHD_FIELD;
        F_Ctxsw    : LW_PRISCV_RISCV_IRQTYPE_CTXSW_FIELD;
        F_Halt    : LW_PRISCV_RISCV_IRQTYPE_HALT_FIELD;
        F_Exterr    : LW_PRISCV_RISCV_IRQTYPE_EXTERR_FIELD;
        F_Swgen0    : LW_PRISCV_RISCV_IRQTYPE_SWGEN0_FIELD;
        F_Swgen1    : LW_PRISCV_RISCV_IRQTYPE_SWGEN1_FIELD;
        F_Ext_Extirq1    : LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ1_FIELD;
        F_Ext_Extirq2    : LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ2_FIELD;
        F_Ext_Extirq3    : LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ3_FIELD;
        F_Ext_Extirq4    : LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ4_FIELD;
        F_Ext_Extirq5    : LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ5_FIELD;
        F_Ext_Extirq6    : LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ6_FIELD;
        F_Ext_Extirq7    : LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ7_FIELD;
        F_Ext_Extirq8    : LW_PRISCV_RISCV_IRQTYPE_EXT_EXTIRQ8_FIELD;
        F_Dma    : LW_PRISCV_RISCV_IRQTYPE_DMA_FIELD;
        F_Sha    : LW_PRISCV_RISCV_IRQTYPE_SHA_FIELD;
        F_Memerr    : LW_PRISCV_RISCV_IRQTYPE_MEMERR_FIELD;
        F_Icd    : LW_PRISCV_RISCV_IRQTYPE_ICD_FIELD;
        F_Iopmp    : LW_PRISCV_RISCV_IRQTYPE_IOPMP_FIELD;
        F_Core_Mismatch    : LW_PRISCV_RISCV_IRQTYPE_CORE_MISMATCH_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IRQTYPE_REGISTER use
    record
        F_Gptmr at 0 range 0 .. 0;
        F_Wdtmr at 0 range 1 .. 1;
        F_Mthd at 0 range 2 .. 2;
        F_Ctxsw at 0 range 3 .. 3;
        F_Halt at 0 range 4 .. 4;
        F_Exterr at 0 range 5 .. 5;
        F_Swgen0 at 0 range 6 .. 6;
        F_Swgen1 at 0 range 7 .. 7;
        F_Ext_Extirq1 at 0 range 8 .. 8;
        F_Ext_Extirq2 at 0 range 9 .. 9;
        F_Ext_Extirq3 at 0 range 10 .. 10;
        F_Ext_Extirq4 at 0 range 11 .. 11;
        F_Ext_Extirq5 at 0 range 12 .. 12;
        F_Ext_Extirq6 at 0 range 13 .. 13;
        F_Ext_Extirq7 at 0 range 14 .. 14;
        F_Ext_Extirq8 at 0 range 15 .. 15;
        F_Dma at 0 range 16 .. 16;
        F_Sha at 0 range 17 .. 17;
        F_Memerr at 0 range 18 .. 18;
        F_Icd at 0 range 22 .. 22;
        F_Iopmp at 0 range 23 .. 23;
        F_Core_Mismatch at 0 range 24 .. 24;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Irqdeleg    : constant Bar0_Addr := 16#0000_0534#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_GPTMR_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Gptmr_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Gptmr_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_GPTMR_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Gptmr_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Gptmr_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_WDTMR_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Wdtmr_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Wdtmr_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_WDTMR_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Wdtmr_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Wdtmr_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_MTHD_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Mthd_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Mthd_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_MTHD_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Mthd_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Mthd_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_CTXSW_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Ctxsw_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Ctxsw_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_CTXSW_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Ctxsw_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Ctxsw_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_HALT_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Halt_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Halt_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_HALT_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Halt_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Halt_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_EXTERR_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Exterr_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Exterr_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_EXTERR_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Exterr_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Exterr_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_SWGEN0_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Swgen0_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Swgen0_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_SWGEN0_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Swgen0_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Swgen0_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_SWGEN1_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Swgen1_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Swgen1_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_SWGEN1_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Swgen1_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Swgen1_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ1_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq1_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq1_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ1_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq1_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq1_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ2_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq2_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq2_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ2_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq2_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq2_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ3_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq3_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq3_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ3_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq3_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq3_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ4_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq4_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq4_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ4_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq4_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq4_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ5_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq5_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq5_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ5_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq5_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq5_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ6_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq6_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq6_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ6_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq6_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq6_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ7_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq7_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq7_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ7_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq7_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq7_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ8_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq8_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq8_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ8_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq8_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq8_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_DMA_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Dma_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Dma_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_DMA_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Dma_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Dma_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_SHA_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Sha_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Sha_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_SHA_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Sha_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Sha_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_MEMERR_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Memerr_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Memerr_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_MEMERR_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Memerr_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Memerr_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_ICD_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Icd_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Icd_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_ICD_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Icd_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Icd_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_IOPMP_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Iopmp_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Iopmp_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_IOPMP_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Iopmp_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Iopmp_Riscv_Seip => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_CORE_MISMATCH_FIELD is
    (
        Lw_Priscv_Riscv_Irqdeleg_Core_Mismatch_Riscv_Meip,
        Lw_Priscv_Riscv_Irqdeleg_Core_Mismatch_Riscv_Seip
    ) with size => 1;

    for LW_PRISCV_RISCV_IRQDELEG_CORE_MISMATCH_FIELD use
    (
        Lw_Priscv_Riscv_Irqdeleg_Core_Mismatch_Riscv_Meip => 0,
        Lw_Priscv_Riscv_Irqdeleg_Core_Mismatch_Riscv_Seip => 1
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IRQDELEG_REGISTER is
    record
        F_Gptmr    : LW_PRISCV_RISCV_IRQDELEG_GPTMR_FIELD;
        F_Wdtmr    : LW_PRISCV_RISCV_IRQDELEG_WDTMR_FIELD;
        F_Mthd    : LW_PRISCV_RISCV_IRQDELEG_MTHD_FIELD;
        F_Ctxsw    : LW_PRISCV_RISCV_IRQDELEG_CTXSW_FIELD;
        F_Halt    : LW_PRISCV_RISCV_IRQDELEG_HALT_FIELD;
        F_Exterr    : LW_PRISCV_RISCV_IRQDELEG_EXTERR_FIELD;
        F_Swgen0    : LW_PRISCV_RISCV_IRQDELEG_SWGEN0_FIELD;
        F_Swgen1    : LW_PRISCV_RISCV_IRQDELEG_SWGEN1_FIELD;
        F_Ext_Extirq1    : LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ1_FIELD;
        F_Ext_Extirq2    : LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ2_FIELD;
        F_Ext_Extirq3    : LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ3_FIELD;
        F_Ext_Extirq4    : LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ4_FIELD;
        F_Ext_Extirq5    : LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ5_FIELD;
        F_Ext_Extirq6    : LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ6_FIELD;
        F_Ext_Extirq7    : LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ7_FIELD;
        F_Ext_Extirq8    : LW_PRISCV_RISCV_IRQDELEG_EXT_EXTIRQ8_FIELD;
        F_Dma    : LW_PRISCV_RISCV_IRQDELEG_DMA_FIELD;
        F_Sha    : LW_PRISCV_RISCV_IRQDELEG_SHA_FIELD;
        F_Memerr    : LW_PRISCV_RISCV_IRQDELEG_MEMERR_FIELD;
        F_Icd    : LW_PRISCV_RISCV_IRQDELEG_ICD_FIELD;
        F_Iopmp    : LW_PRISCV_RISCV_IRQDELEG_IOPMP_FIELD;
        F_Core_Mismatch    : LW_PRISCV_RISCV_IRQDELEG_CORE_MISMATCH_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IRQDELEG_REGISTER use
    record
        F_Gptmr at 0 range 0 .. 0;
        F_Wdtmr at 0 range 1 .. 1;
        F_Mthd at 0 range 2 .. 2;
        F_Ctxsw at 0 range 3 .. 3;
        F_Halt at 0 range 4 .. 4;
        F_Exterr at 0 range 5 .. 5;
        F_Swgen0 at 0 range 6 .. 6;
        F_Swgen1 at 0 range 7 .. 7;
        F_Ext_Extirq1 at 0 range 8 .. 8;
        F_Ext_Extirq2 at 0 range 9 .. 9;
        F_Ext_Extirq3 at 0 range 10 .. 10;
        F_Ext_Extirq4 at 0 range 11 .. 11;
        F_Ext_Extirq5 at 0 range 12 .. 12;
        F_Ext_Extirq6 at 0 range 13 .. 13;
        F_Ext_Extirq7 at 0 range 14 .. 14;
        F_Ext_Extirq8 at 0 range 15 .. 15;
        F_Dma at 0 range 16 .. 16;
        F_Sha at 0 range 17 .. 17;
        F_Memerr at 0 range 18 .. 18;
        F_Icd at 0 range 22 .. 22;
        F_Iopmp at 0 range 23 .. 23;
        F_Core_Mismatch at 0 range 24 .. 24;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Boot_Vector_Hi    : constant Bar0_Addr := 16#0000_0384#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_BOOT_VECTOR_HI_VECTOR_FIELD is new LwU32;
    Lw_Priscv_Riscv_Boot_Vector_Hi_Vector_Init :
        constant LW_PRISCV_RISCV_BOOT_VECTOR_HI_VECTOR_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BOOT_VECTOR_HI_REGISTER is
    record
        F_Vector    : LW_PRISCV_RISCV_BOOT_VECTOR_HI_VECTOR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BOOT_VECTOR_HI_REGISTER use
    record
        F_Vector at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Boot_Vector_Lo    : constant Bar0_Addr := 16#0000_0380#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_BOOT_VECTOR_LO_VECTOR_FIELD is new LwU32;
    Lw_Priscv_Riscv_Boot_Vector_Lo_Vector_Init :
        constant LW_PRISCV_RISCV_BOOT_VECTOR_LO_VECTOR_FIELD
        := 16#0008_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BOOT_VECTOR_LO_REGISTER is
    record
        F_Vector    : LW_PRISCV_RISCV_BOOT_VECTOR_LO_VECTOR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BOOT_VECTOR_LO_REGISTER use
    record
        F_Vector at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Cpuctl    : constant Bar0_Addr := 16#0000_0388#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_CPUCTL_STARTCPU_FIELD is
    (
        Lw_Priscv_Riscv_Cpuctl_Startcpu_False,
        Lw_Priscv_Riscv_Cpuctl_Startcpu_True
    ) with size => 1;

    for LW_PRISCV_RISCV_CPUCTL_STARTCPU_FIELD use
    (
        Lw_Priscv_Riscv_Cpuctl_Startcpu_False => 0,
        Lw_Priscv_Riscv_Cpuctl_Startcpu_True => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_CPUCTL_HALTED_FIELD is new LwU1;
    Lw_Priscv_Riscv_Cpuctl_Halted_Init :
        constant LW_PRISCV_RISCV_CPUCTL_HALTED_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_CPUCTL_STOPPED_FIELD is new LwU1;
    Lw_Priscv_Riscv_Cpuctl_Stopped_Init :
        constant LW_PRISCV_RISCV_CPUCTL_STOPPED_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_CPUCTL_ACTIVE_STAT_FIELD is
    (
        Lw_Priscv_Riscv_Cpuctl_Active_Stat_Inactive,
        Lw_Priscv_Riscv_Cpuctl_Active_Stat_Active
    ) with size => 1;

    for LW_PRISCV_RISCV_CPUCTL_ACTIVE_STAT_FIELD use
    (
        Lw_Priscv_Riscv_Cpuctl_Active_Stat_Inactive => 0,
        Lw_Priscv_Riscv_Cpuctl_Active_Stat_Active => 1
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_CPUCTL_REGISTER is
    record
        F_Startcpu    : LW_PRISCV_RISCV_CPUCTL_STARTCPU_FIELD;
        F_Halted    : LW_PRISCV_RISCV_CPUCTL_HALTED_FIELD;
        F_Stopped    : LW_PRISCV_RISCV_CPUCTL_STOPPED_FIELD;
        F_Active_Stat    : LW_PRISCV_RISCV_CPUCTL_ACTIVE_STAT_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_CPUCTL_REGISTER use
    record
        F_Startcpu at 0 range 0 .. 0;
        F_Halted at 0 range 4 .. 4;
        F_Stopped at 0 range 5 .. 5;
        F_Active_Stat at 0 range 7 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Lwconfig    : constant Bar0_Addr := 16#0000_0390#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_WR_ALLOC_EN_FIELD is new LwU1;
    Lw_Priscv_Riscv_Lwconfig_Wr_Alloc_En_Rst :
        constant LW_PRISCV_RISCV_LWCONFIG_WR_ALLOC_EN_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_LD_MERGE_EN_FIELD is new LwU1;
    Lw_Priscv_Riscv_Lwconfig_Ld_Merge_En_Rst :
        constant LW_PRISCV_RISCV_LWCONFIG_LD_MERGE_EN_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_ST_WAIT_CSB_ACK_EN_FIELD is new LwU1;
    Lw_Priscv_Riscv_Lwconfig_St_Wait_Csb_Ack_En_Rst :
        constant LW_PRISCV_RISCV_LWCONFIG_ST_WAIT_CSB_ACK_EN_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_ST_WAIT_FBIF_ACK_EN_FIELD is new LwU1;
    Lw_Priscv_Riscv_Lwconfig_St_Wait_Fbif_Ack_En_Rst :
        constant LW_PRISCV_RISCV_LWCONFIG_ST_WAIT_FBIF_ACK_EN_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_VB_FWD_EN_FIELD is new LwU1;
    Lw_Priscv_Riscv_Lwconfig_Vb_Fwd_En_Rst :
        constant LW_PRISCV_RISCV_LWCONFIG_VB_FWD_EN_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_WRITE_BACK_EN_FIELD is new LwU1;
    Lw_Priscv_Riscv_Lwconfig_Write_Back_En_Rst :
        constant LW_PRISCV_RISCV_LWCONFIG_WRITE_BACK_EN_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_ST_COMMIT_ISSUE_EN_FIELD is new LwU1;
    Lw_Priscv_Riscv_Lwconfig_St_Commit_Issue_En_Rst :
        constant LW_PRISCV_RISCV_LWCONFIG_ST_COMMIT_ISSUE_EN_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_TLB_FIX_SIZE_EN_FIELD is new LwU1;
    Lw_Priscv_Riscv_Lwconfig_Tlb_Fix_Size_En_Rst :
        constant LW_PRISCV_RISCV_LWCONFIG_TLB_FIX_SIZE_EN_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_SYSOP_CSR_EN_FIELD is new LwU1;
    Lw_Priscv_Riscv_Lwconfig_Sysop_Csr_En_Rst :
        constant LW_PRISCV_RISCV_LWCONFIG_SYSOP_CSR_EN_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_FENCE_FBFLUSH_EN_FIELD is new LwU1;
    Lw_Priscv_Riscv_Lwconfig_Fence_Fbflush_En_Rst :
        constant LW_PRISCV_RISCV_LWCONFIG_FENCE_FBFLUSH_EN_FIELD
        := 16#0000_0001#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_LWCONFIG_REGISTER is
    record
        F_Wr_Alloc_En    : LW_PRISCV_RISCV_LWCONFIG_WR_ALLOC_EN_FIELD;
        F_Ld_Merge_En    : LW_PRISCV_RISCV_LWCONFIG_LD_MERGE_EN_FIELD;
        F_St_Wait_Csb_Ack_En    : LW_PRISCV_RISCV_LWCONFIG_ST_WAIT_CSB_ACK_EN_FIELD;
        F_St_Wait_Fbif_Ack_En    : LW_PRISCV_RISCV_LWCONFIG_ST_WAIT_FBIF_ACK_EN_FIELD;
        F_Vb_Fwd_En    : LW_PRISCV_RISCV_LWCONFIG_VB_FWD_EN_FIELD;
        F_Write_Back_En    : LW_PRISCV_RISCV_LWCONFIG_WRITE_BACK_EN_FIELD;
        F_St_Commit_Issue_En    : LW_PRISCV_RISCV_LWCONFIG_ST_COMMIT_ISSUE_EN_FIELD;
        F_Tlb_Fix_Size_En    : LW_PRISCV_RISCV_LWCONFIG_TLB_FIX_SIZE_EN_FIELD;
        F_Sysop_Csr_En    : LW_PRISCV_RISCV_LWCONFIG_SYSOP_CSR_EN_FIELD;
        F_Fence_Fbflush_En    : LW_PRISCV_RISCV_LWCONFIG_FENCE_FBFLUSH_EN_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_LWCONFIG_REGISTER use
    record
        F_Wr_Alloc_En at 0 range 0 .. 0;
        F_Ld_Merge_En at 0 range 1 .. 1;
        F_St_Wait_Csb_Ack_En at 0 range 2 .. 2;
        F_St_Wait_Fbif_Ack_En at 0 range 3 .. 3;
        F_Vb_Fwd_En at 0 range 4 .. 4;
        F_Write_Back_En at 0 range 6 .. 6;
        F_St_Commit_Issue_En at 0 range 7 .. 7;
        F_Tlb_Fix_Size_En at 0 range 8 .. 8;
        F_Sysop_Csr_En at 0 range 9 .. 9;
        F_Fence_Fbflush_En at 0 range 10 .. 10;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Cg    : constant Bar0_Addr := 16#0000_0398#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_CG_SLCG_FIELD is
    (
        Lw_Priscv_Riscv_Cg_Slcg_Disable,
        Lw_Priscv_Riscv_Cg_Slcg_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_CG_SLCG_FIELD use
    (
        Lw_Priscv_Riscv_Cg_Slcg_Disable => 0,
        Lw_Priscv_Riscv_Cg_Slcg_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_CG_CORE_SLCG_FIELD is
    (
        Lw_Priscv_Riscv_Cg_Core_Slcg_Disable,
        Lw_Priscv_Riscv_Cg_Core_Slcg_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_CG_CORE_SLCG_FIELD use
    (
        Lw_Priscv_Riscv_Cg_Core_Slcg_Disable => 0,
        Lw_Priscv_Riscv_Cg_Core_Slcg_Enable => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_CG_RSVD_FIELD is new LwU30;
    Lw_Priscv_Riscv_Cg_Rsvd_Init :
        constant LW_PRISCV_RISCV_CG_RSVD_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_CG_REGISTER is
    record
        F_Slcg    : LW_PRISCV_RISCV_CG_SLCG_FIELD;
        F_Core_Slcg    : LW_PRISCV_RISCV_CG_CORE_SLCG_FIELD;
        F_Rsvd    : LW_PRISCV_RISCV_CG_RSVD_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_CG_REGISTER use
    record
        F_Slcg at 0 range 0 .. 0;
        F_Core_Slcg at 0 range 1 .. 1;
        F_Rsvd at 0 range 2 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Cya    : constant Bar0_Addr := 16#0000_039C#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_CYA_RSVD_FIELD is new LwU32;
    Lw_Priscv_Riscv_Cya_Rsvd_Init :
        constant LW_PRISCV_RISCV_CYA_RSVD_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_CYA_REGISTER is
    record
        F_Rsvd    : LW_PRISCV_RISCV_CYA_RSVD_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_CYA_REGISTER use
    record
        F_Rsvd at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Dbgctl    : constant Bar0_Addr := 16#0000_03C8#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_STOP_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Stop_Disable,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Stop_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_STOP_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Stop_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Stop_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RUN_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Run_Disable,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Run_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RUN_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Run_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Run_Enable => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_RSVD1_FIELD is new LwU1;
    Lw_Priscv_Riscv_Dbgctl_Rsvd1_Init :
        constant LW_PRISCV_RISCV_DBGCTL_RSVD1_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_STEP_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Step_Disable,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Step_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_STEP_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Step_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Step_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_J_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_J_Disable,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_J_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_J_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_J_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_J_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_EMASK_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Emask_Disable,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Emask_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_EMASK_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Emask_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Emask_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RREG_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rreg_Disable,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rreg_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RREG_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rreg_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rreg_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_WREG_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Wreg_Disable,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Wreg_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_WREG_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Wreg_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Wreg_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RDM_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rdm_Disable,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rdm_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RDM_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rdm_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rdm_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_WDM_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Wdm_Disable,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Wdm_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_WDM_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Wdm_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Wdm_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RSTAT_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rstat_Disable,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rstat_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RSTAT_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rstat_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rstat_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_IBRKPT_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Ibrkpt_Disable,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Ibrkpt_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_IBRKPT_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Ibrkpt_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Ibrkpt_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RCSR_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rcsr_Disable,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rcsr_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RCSR_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rcsr_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rcsr_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_WCSR_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Wcsr_Disable,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Wcsr_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_WCSR_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Wcsr_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Wcsr_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RPC_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rpc_Disable,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rpc_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RPC_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rpc_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rpc_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RFREG_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rfreg_Disable,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rfreg_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RFREG_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rfreg_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Rfreg_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_WFREG_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Wfreg_Disable,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Wfreg_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_WFREG_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Wfreg_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Icd_Cmdwl_Wfreg_Enable => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_RSVD_FIELD is new LwU13;
    Lw_Priscv_Riscv_Dbgctl_Rsvd_Init :
        constant LW_PRISCV_RISCV_DBGCTL_RSVD_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_START_IN_ICD_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Start_In_Icd_False,
        Lw_Priscv_Riscv_Dbgctl_Start_In_Icd_True
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_START_IN_ICD_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Start_In_Icd_False => 0,
        Lw_Priscv_Riscv_Dbgctl_Start_In_Icd_True => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_SINGLE_STEP_MODE_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Single_Step_Mode_Disable,
        Lw_Priscv_Riscv_Dbgctl_Single_Step_Mode_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_SINGLE_STEP_MODE_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Single_Step_Mode_Disable => 0,
        Lw_Priscv_Riscv_Dbgctl_Single_Step_Mode_Enable => 1
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_REGISTER is
    record
        F_Icd_Cmdwl_Stop    : LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_STOP_FIELD;
        F_Icd_Cmdwl_Run    : LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RUN_FIELD;
        F_Rsvd1    : LW_PRISCV_RISCV_DBGCTL_RSVD1_FIELD;
        F_Icd_Cmdwl_Step    : LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_STEP_FIELD;
        F_Icd_Cmdwl_J    : LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_J_FIELD;
        F_Icd_Cmdwl_Emask    : LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_EMASK_FIELD;
        F_Icd_Cmdwl_Rreg    : LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RREG_FIELD;
        F_Icd_Cmdwl_Wreg    : LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_WREG_FIELD;
        F_Icd_Cmdwl_Rdm    : LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RDM_FIELD;
        F_Icd_Cmdwl_Wdm    : LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_WDM_FIELD;
        F_Icd_Cmdwl_Rstat    : LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RSTAT_FIELD;
        F_Icd_Cmdwl_Ibrkpt    : LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_IBRKPT_FIELD;
        F_Icd_Cmdwl_Rcsr    : LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RCSR_FIELD;
        F_Icd_Cmdwl_Wcsr    : LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_WCSR_FIELD;
        F_Icd_Cmdwl_Rpc    : LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RPC_FIELD;
        F_Icd_Cmdwl_Rfreg    : LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_RFREG_FIELD;
        F_Icd_Cmdwl_Wfreg    : LW_PRISCV_RISCV_DBGCTL_ICD_CMDWL_WFREG_FIELD;
        F_Rsvd    : LW_PRISCV_RISCV_DBGCTL_RSVD_FIELD;
        F_Start_In_Icd    : LW_PRISCV_RISCV_DBGCTL_START_IN_ICD_FIELD;
        F_Single_Step_Mode    : LW_PRISCV_RISCV_DBGCTL_SINGLE_STEP_MODE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_DBGCTL_REGISTER use
    record
        F_Icd_Cmdwl_Stop at 0 range 0 .. 0;
        F_Icd_Cmdwl_Run at 0 range 1 .. 1;
        F_Rsvd1 at 0 range 2 .. 2;
        F_Icd_Cmdwl_Step at 0 range 3 .. 3;
        F_Icd_Cmdwl_J at 0 range 4 .. 4;
        F_Icd_Cmdwl_Emask at 0 range 5 .. 5;
        F_Icd_Cmdwl_Rreg at 0 range 6 .. 6;
        F_Icd_Cmdwl_Wreg at 0 range 7 .. 7;
        F_Icd_Cmdwl_Rdm at 0 range 8 .. 8;
        F_Icd_Cmdwl_Wdm at 0 range 9 .. 9;
        F_Icd_Cmdwl_Rstat at 0 range 10 .. 10;
        F_Icd_Cmdwl_Ibrkpt at 0 range 11 .. 11;
        F_Icd_Cmdwl_Rcsr at 0 range 12 .. 12;
        F_Icd_Cmdwl_Wcsr at 0 range 13 .. 13;
        F_Icd_Cmdwl_Rpc at 0 range 14 .. 14;
        F_Icd_Cmdwl_Rfreg at 0 range 15 .. 15;
        F_Icd_Cmdwl_Wfreg at 0 range 16 .. 16;
        F_Rsvd at 0 range 17 .. 29;
        F_Start_In_Icd at 0 range 30 .. 30;
        F_Single_Step_Mode at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Dbgctl_Lock    : constant Bar0_Addr := 16#0000_03CC#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_STOP_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Stop_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Stop_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_STOP_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Stop_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Stop_Locked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RUN_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Run_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Run_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RUN_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Run_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Run_Locked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_RSVD1_FIELD is new LwU1;
    Lw_Priscv_Riscv_Dbgctl_Lock_Rsvd1_Init :
        constant LW_PRISCV_RISCV_DBGCTL_LOCK_RSVD1_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_STEP_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Step_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Step_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_STEP_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Step_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Step_Locked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_J_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_J_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_J_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_J_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_J_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_J_Locked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_EMASK_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Emask_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Emask_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_EMASK_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Emask_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Emask_Locked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RREG_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rreg_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rreg_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RREG_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rreg_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rreg_Locked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_WREG_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Wreg_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Wreg_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_WREG_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Wreg_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Wreg_Locked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RDM_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rdm_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rdm_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RDM_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rdm_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rdm_Locked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_WDM_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Wdm_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Wdm_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_WDM_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Wdm_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Wdm_Locked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RSTAT_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rstat_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rstat_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RSTAT_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rstat_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rstat_Locked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_IBRKPT_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Ibrkpt_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Ibrkpt_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_IBRKPT_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Ibrkpt_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Ibrkpt_Locked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RCSR_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rcsr_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rcsr_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RCSR_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rcsr_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rcsr_Locked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_WCSR_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Wcsr_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Wcsr_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_WCSR_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Wcsr_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Wcsr_Locked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RPC_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rpc_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rpc_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RPC_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rpc_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rpc_Locked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RFREG_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rfreg_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rfreg_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RFREG_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rfreg_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Rfreg_Locked => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_WFREG_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Wfreg_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Wfreg_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_WFREG_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Wfreg_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Icd_Cmdwl_Wfreg_Locked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_RSVD_FIELD is new LwU14;
    Lw_Priscv_Riscv_Dbgctl_Lock_Rsvd_Init :
        constant LW_PRISCV_RISCV_DBGCTL_LOCK_RSVD_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_SINGLE_STEP_MODE_FIELD is
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Single_Step_Mode_Unlocked,
        Lw_Priscv_Riscv_Dbgctl_Lock_Single_Step_Mode_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_SINGLE_STEP_MODE_FIELD use
    (
        Lw_Priscv_Riscv_Dbgctl_Lock_Single_Step_Mode_Unlocked => 0,
        Lw_Priscv_Riscv_Dbgctl_Lock_Single_Step_Mode_Locked => 1
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_DBGCTL_LOCK_REGISTER is
    record
        F_Icd_Cmdwl_Stop    : LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_STOP_FIELD;
        F_Icd_Cmdwl_Run    : LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RUN_FIELD;
        F_Rsvd1    : LW_PRISCV_RISCV_DBGCTL_LOCK_RSVD1_FIELD;
        F_Icd_Cmdwl_Step    : LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_STEP_FIELD;
        F_Icd_Cmdwl_J    : LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_J_FIELD;
        F_Icd_Cmdwl_Emask    : LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_EMASK_FIELD;
        F_Icd_Cmdwl_Rreg    : LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RREG_FIELD;
        F_Icd_Cmdwl_Wreg    : LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_WREG_FIELD;
        F_Icd_Cmdwl_Rdm    : LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RDM_FIELD;
        F_Icd_Cmdwl_Wdm    : LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_WDM_FIELD;
        F_Icd_Cmdwl_Rstat    : LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RSTAT_FIELD;
        F_Icd_Cmdwl_Ibrkpt    : LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_IBRKPT_FIELD;
        F_Icd_Cmdwl_Rcsr    : LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RCSR_FIELD;
        F_Icd_Cmdwl_Wcsr    : LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_WCSR_FIELD;
        F_Icd_Cmdwl_Rpc    : LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RPC_FIELD;
        F_Icd_Cmdwl_Rfreg    : LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_RFREG_FIELD;
        F_Icd_Cmdwl_Wfreg    : LW_PRISCV_RISCV_DBGCTL_LOCK_ICD_CMDWL_WFREG_FIELD;
        F_Rsvd    : LW_PRISCV_RISCV_DBGCTL_LOCK_RSVD_FIELD;
        F_Single_Step_Mode    : LW_PRISCV_RISCV_DBGCTL_LOCK_SINGLE_STEP_MODE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_DBGCTL_LOCK_REGISTER use
    record
        F_Icd_Cmdwl_Stop at 0 range 0 .. 0;
        F_Icd_Cmdwl_Run at 0 range 1 .. 1;
        F_Rsvd1 at 0 range 2 .. 2;
        F_Icd_Cmdwl_Step at 0 range 3 .. 3;
        F_Icd_Cmdwl_J at 0 range 4 .. 4;
        F_Icd_Cmdwl_Emask at 0 range 5 .. 5;
        F_Icd_Cmdwl_Rreg at 0 range 6 .. 6;
        F_Icd_Cmdwl_Wreg at 0 range 7 .. 7;
        F_Icd_Cmdwl_Rdm at 0 range 8 .. 8;
        F_Icd_Cmdwl_Wdm at 0 range 9 .. 9;
        F_Icd_Cmdwl_Rstat at 0 range 10 .. 10;
        F_Icd_Cmdwl_Ibrkpt at 0 range 11 .. 11;
        F_Icd_Cmdwl_Rcsr at 0 range 12 .. 12;
        F_Icd_Cmdwl_Wcsr at 0 range 13 .. 13;
        F_Icd_Cmdwl_Rpc at 0 range 14 .. 14;
        F_Icd_Cmdwl_Rfreg at 0 range 15 .. 15;
        F_Icd_Cmdwl_Wfreg at 0 range 16 .. 16;
        F_Rsvd at 0 range 17 .. 30;
        F_Single_Step_Mode at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Icd_Cmd    : constant Bar0_Addr := 16#0000_03D0#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_CMD_OPC_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Stop,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Run,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Step,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_J,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Emask,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Rreg,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Wreg,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Rdm,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Wdm,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Rstat,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Rcsr,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Wcsr,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Rpc,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Rfreg,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Wfreg
    ) with size => 5;

    for LW_PRISCV_RISCV_ICD_CMD_OPC_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Stop => 0,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Run => 1,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Step => 5,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_J => 6,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Emask => 7,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Rreg => 8,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Wreg => 9,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Rdm => 10,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Wdm => 11,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Rstat => 14,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Rcsr => 16,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Wcsr => 17,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Rpc => 18,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Rfreg => 19,
        Lw_Priscv_Riscv_Icd_Cmd_Opc_Wfreg => 20
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_CMD_SZ_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Cmd_Sz_B,
        Lw_Priscv_Riscv_Icd_Cmd_Sz_Hw,
        Lw_Priscv_Riscv_Icd_Cmd_Sz_W,
        Lw_Priscv_Riscv_Icd_Cmd_Sz_Dw
    ) with size => 2;

    for LW_PRISCV_RISCV_ICD_CMD_SZ_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Cmd_Sz_B => 0,
        Lw_Priscv_Riscv_Icd_Cmd_Sz_Hw => 1,
        Lw_Priscv_Riscv_Icd_Cmd_Sz_W => 2,
        Lw_Priscv_Riscv_Icd_Cmd_Sz_Dw => 3
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_CMD_IDX_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg0,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg1,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg2,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg3,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg4,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg5,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg6,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg7,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg8,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg9,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg10,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg11,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg12,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg13,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg14,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg15,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg16,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg17,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg18,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg19,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg20,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg21,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg22,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg23,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg24,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg25,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg26,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg27,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg28,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg29,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg31
    ) with size => 5;

    for LW_PRISCV_RISCV_ICD_CMD_IDX_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg0 => 0,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg1 => 1,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg2 => 2,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg3 => 3,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg4 => 4,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg5 => 5,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg6 => 6,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg7 => 7,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg8 => 8,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg9 => 9,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg10 => 10,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg11 => 11,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg12 => 12,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg13 => 13,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg14 => 14,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg15 => 15,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg16 => 16,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg17 => 17,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg18 => 18,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg19 => 19,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg20 => 20,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg21 => 21,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg22 => 22,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg23 => 23,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg24 => 24,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg25 => 25,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg26 => 26,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg27 => 27,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg28 => 28,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg29 => 30,
        Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg31 => 31
    );

    Lw_Priscv_Riscv_Icd_Cmd_Idx_Rstat3 :
        constant LW_PRISCV_RISCV_ICD_CMD_IDX_FIELD
        := Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg3;

    Lw_Priscv_Riscv_Icd_Cmd_Idx_Rstat4 :
        constant LW_PRISCV_RISCV_ICD_CMD_IDX_FIELD
        := Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg4;

    Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg30 :
        constant LW_PRISCV_RISCV_ICD_CMD_IDX_FIELD
        := Lw_Priscv_Riscv_Icd_Cmd_Idx_Reg29;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_CMD_ERROR_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Cmd_Error_False,
        Lw_Priscv_Riscv_Icd_Cmd_Error_True
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_CMD_ERROR_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Cmd_Error_False => 0,
        Lw_Priscv_Riscv_Icd_Cmd_Error_True => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_CMD_BUSY_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Cmd_Busy_False,
        Lw_Priscv_Riscv_Icd_Cmd_Busy_True
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_CMD_BUSY_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Cmd_Busy_False => 0,
        Lw_Priscv_Riscv_Icd_Cmd_Busy_True => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_CMD_PARM_FIELD is new LwU16;
    Lw_Priscv_Riscv_Icd_Cmd_Parm_Init :
        constant LW_PRISCV_RISCV_ICD_CMD_PARM_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_ICD_CMD_REGISTER is
    record
        F_Opc    : LW_PRISCV_RISCV_ICD_CMD_OPC_FIELD;
        F_Sz    : LW_PRISCV_RISCV_ICD_CMD_SZ_FIELD;
        F_Idx    : LW_PRISCV_RISCV_ICD_CMD_IDX_FIELD;
        F_Error    : LW_PRISCV_RISCV_ICD_CMD_ERROR_FIELD;
        F_Busy    : LW_PRISCV_RISCV_ICD_CMD_BUSY_FIELD;
        F_Parm    : LW_PRISCV_RISCV_ICD_CMD_PARM_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_ICD_CMD_REGISTER use
    record
        F_Opc at 0 range 0 .. 4;
        F_Sz at 0 range 6 .. 7;
        F_Idx at 0 range 8 .. 12;
        F_Error at 0 range 14 .. 14;
        F_Busy at 0 range 15 .. 15;
        F_Parm at 0 range 16 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Icd_Addr0    : constant Bar0_Addr := 16#0000_03D4#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_ADDR0_ADDR_FIELD is new LwU32;
    Lw_Priscv_Riscv_Icd_Addr0_Addr_Init :
        constant LW_PRISCV_RISCV_ICD_ADDR0_ADDR_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_ICD_ADDR0_REGISTER is
    record
        F_Addr    : LW_PRISCV_RISCV_ICD_ADDR0_ADDR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_ICD_ADDR0_REGISTER use
    record
        F_Addr at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Icd_Addr1    : constant Bar0_Addr := 16#0000_03D8#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_ADDR1_ADDR_FIELD is new LwU32;
    Lw_Priscv_Riscv_Icd_Addr1_Addr_Init :
        constant LW_PRISCV_RISCV_ICD_ADDR1_ADDR_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_ICD_ADDR1_REGISTER is
    record
        F_Addr    : LW_PRISCV_RISCV_ICD_ADDR1_ADDR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_ICD_ADDR1_REGISTER use
    record
        F_Addr at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Icd_Wdata0    : constant Bar0_Addr := 16#0000_03DC#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_WDATA0_DATA_FIELD is new LwU32;
    Lw_Priscv_Riscv_Icd_Wdata0_Data_Init :
        constant LW_PRISCV_RISCV_ICD_WDATA0_DATA_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_ICD_WDATA0_REGISTER is
    record
        F_Data    : LW_PRISCV_RISCV_ICD_WDATA0_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_ICD_WDATA0_REGISTER use
    record
        F_Data at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Icd_Wdata1    : constant Bar0_Addr := 16#0000_03E0#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_WDATA1_DATA_FIELD is new LwU32;
    Lw_Priscv_Riscv_Icd_Wdata1_Data_Init :
        constant LW_PRISCV_RISCV_ICD_WDATA1_DATA_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_ICD_WDATA1_REGISTER is
    record
        F_Data    : LW_PRISCV_RISCV_ICD_WDATA1_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_ICD_WDATA1_REGISTER use
    record
        F_Data at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Icd_Rdata0    : constant Bar0_Addr := 16#0000_03E4#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_DATA_FIELD is new LwU32;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_ROB_FULL_FIELD is new LwU1;
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT4_ICD_STATE_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat4_Icd_State_Active,
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat4_Icd_State_Inactive,
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat4_Icd_State_Icd
    ) with size => 2;

    for LW_PRISCV_RISCV_ICD_RDATA0_RSTAT4_ICD_STATE_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat4_Icd_State_Active => 0,
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat4_Icd_State_Inactive => 1,
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat4_Icd_State_Icd => 2
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_IFU_INSTBUF_FULL_FIELD is new LwU1;
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_SSINT_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Ssint_Expt,
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Ssint_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_SSINT_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Ssint_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Ssint_Enter_Icd => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_IFU_LINEFILL_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT4_RS0_FIELD is new LwU2;
    Lw_Priscv_Riscv_Icd_Rdata0_Rstat4_Rs0_Value :
        constant LW_PRISCV_RISCV_ICD_RDATA0_RSTAT4_RS0_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_LSU_STB_FULL_FIELD is new LwU1;
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_MSINT_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Msint_Expt,
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Msint_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_MSINT_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Msint_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Msint_Enter_Icd => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_LSU_MEM_WACK_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT4_TRIGGER_HIT_INDEX_FIELD is new LwU7;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_LSU_IO_WACK_FULL_FIELD is new LwU1;
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_STINT_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Stint_Expt,
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Stint_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_STINT_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Stint_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Stint_Enter_Icd => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_LSU_LINEFILL_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_LSU_VICBUF_FULL_FIELD is new LwU1;
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_MTINT_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Mtint_Expt,
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Mtint_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_MTINT_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Mtint_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Mtint_Enter_Icd => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_LSU_CB_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_ALU_RS_FULL_FIELD is new LwU1;
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_SEINT_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Seint_Expt,
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Seint_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_SEINT_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Seint_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Seint_Enter_Icd => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_IMD_RS_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_FPU_RS_FULL_FIELD is new LwU1;
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_MEINT_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Meint_Expt,
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Meint_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_MEINT_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Meint_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata0_Rstat3_Meint_Enter_Icd => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT4_RS1_FIELD is new LwU21;
    Lw_Priscv_Riscv_Icd_Rdata0_Rstat4_Rs1_Value :
        constant LW_PRISCV_RISCV_ICD_RDATA0_RSTAT4_RS1_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_IOWR_INFLIGHT_FIELD is new LwU8;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_MEMWR_INFLIGHT_FIELD is new LwU8;
---------- Record Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA0_REGISTER_0 is
    record
        F_Data    : LW_PRISCV_RISCV_ICD_RDATA0_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_ICD_RDATA0_REGISTER_0 use
    record
        F_Data at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
    type LW_PRISCV_RISCV_ICD_RDATA0_REGISTER_1 is
    record
        F_Rstat0_Rob_Full    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_ROB_FULL_FIELD;
        F_Rstat0_Ifu_Instbuf_Full    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_IFU_INSTBUF_FULL_FIELD;
        F_Rstat0_Ifu_Linefill_Full    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_IFU_LINEFILL_FULL_FIELD;
        F_Rstat0_Lsu_Stb_Full    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_LSU_STB_FULL_FIELD;
        F_Rstat0_Lsu_Mem_Wack_Full    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_LSU_MEM_WACK_FULL_FIELD;
        F_Rstat0_Lsu_Io_Wack_Full    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_LSU_IO_WACK_FULL_FIELD;
        F_Rstat0_Lsu_Linefill_Full    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_LSU_LINEFILL_FULL_FIELD;
        F_Rstat0_Lsu_Vicbuf_Full    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_LSU_VICBUF_FULL_FIELD;
        F_Rstat0_Lsu_Cb_Full    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_LSU_CB_FULL_FIELD;
        F_Rstat0_Alu_Rs_Full    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_ALU_RS_FULL_FIELD;
        F_Rstat0_Imd_Rs_Full    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_IMD_RS_FULL_FIELD;
        F_Rstat0_Fpu_Rs_Full    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_FPU_RS_FULL_FIELD;
        F_Rstat0_Iowr_Inflight    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_IOWR_INFLIGHT_FIELD;
        F_Rstat0_Memwr_Inflight    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT0_MEMWR_INFLIGHT_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_ICD_RDATA0_REGISTER_1 use
    record
        F_Rstat0_Rob_Full at 0 range 0 .. 0;
        F_Rstat0_Ifu_Instbuf_Full at 0 range 1 .. 1;
        F_Rstat0_Ifu_Linefill_Full at 0 range 2 .. 2;
        F_Rstat0_Lsu_Stb_Full at 0 range 3 .. 3;
        F_Rstat0_Lsu_Mem_Wack_Full at 0 range 4 .. 4;
        F_Rstat0_Lsu_Io_Wack_Full at 0 range 5 .. 5;
        F_Rstat0_Lsu_Linefill_Full at 0 range 6 .. 6;
        F_Rstat0_Lsu_Vicbuf_Full at 0 range 7 .. 7;
        F_Rstat0_Lsu_Cb_Full at 0 range 8 .. 8;
        F_Rstat0_Alu_Rs_Full at 0 range 9 .. 9;
        F_Rstat0_Imd_Rs_Full at 0 range 10 .. 10;
        F_Rstat0_Fpu_Rs_Full at 0 range 11 .. 11;
        F_Rstat0_Iowr_Inflight at 0 range 16 .. 23;
        F_Rstat0_Memwr_Inflight at 0 range 24 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
    type LW_PRISCV_RISCV_ICD_RDATA0_REGISTER_2 is
    record
        F_Rstat3_Ssint    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_SSINT_FIELD;
        F_Rstat3_Msint    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_MSINT_FIELD;
        F_Rstat3_Stint    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_STINT_FIELD;
        F_Rstat3_Mtint    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_MTINT_FIELD;
        F_Rstat3_Seint    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_SEINT_FIELD;
        F_Rstat3_Meint    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT3_MEINT_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_ICD_RDATA0_REGISTER_2 use
    record
        F_Rstat3_Ssint at 0 range 1 .. 1;
        F_Rstat3_Msint at 0 range 3 .. 3;
        F_Rstat3_Stint at 0 range 5 .. 5;
        F_Rstat3_Mtint at 0 range 7 .. 7;
        F_Rstat3_Seint at 0 range 9 .. 9;
        F_Rstat3_Meint at 0 range 11 .. 11;
    end record;

------------------------------------------------------------------------------------------------------
    type LW_PRISCV_RISCV_ICD_RDATA0_REGISTER_3 is
    record
        F_Rstat4_Icd_State    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT4_ICD_STATE_FIELD;
        F_Rstat4_Rs0    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT4_RS0_FIELD;
        F_Rstat4_Trigger_Hit_Index    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT4_TRIGGER_HIT_INDEX_FIELD;
        F_Rstat4_Rs1    : LW_PRISCV_RISCV_ICD_RDATA0_RSTAT4_RS1_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_ICD_RDATA0_REGISTER_3 use
    record
        F_Rstat4_Icd_State at 0 range 0 .. 1;
        F_Rstat4_Rs0 at 0 range 2 .. 3;
        F_Rstat4_Trigger_Hit_Index at 0 range 4 .. 10;
        F_Rstat4_Rs1 at 0 range 11 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Icd_Rdata1    : constant Bar0_Addr := 16#0000_03E8#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_DATA_FIELD is new LwU32;
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_IAMA_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Iama_Expt,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Iama_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_IAMA_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Iama_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Iama_Enter_Icd => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT4_RS_FIELD is new LwU32;
    Lw_Priscv_Riscv_Icd_Rdata1_Rstat4_Rs_Value :
        constant LW_PRISCV_RISCV_ICD_RDATA1_RSTAT4_RS_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_IFAULT_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Ifault_Expt,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Ifault_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_IFAULT_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Ifault_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Ifault_Enter_Icd => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_ILL_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Ill_Expt,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Ill_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_ILL_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Ill_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Ill_Enter_Icd => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_BKPT_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Bkpt_Expt,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Bkpt_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_BKPT_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Bkpt_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Bkpt_Enter_Icd => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_LAMA_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Lama_Expt,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Lama_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_LAMA_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Lama_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Lama_Enter_Icd => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_LFAULT_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Lfault_Expt,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Lfault_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_LFAULT_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Lfault_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Lfault_Enter_Icd => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_SAMA_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Sama_Expt,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Sama_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_SAMA_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Sama_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Sama_Enter_Icd => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_SFAULT_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Sfault_Expt,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Sfault_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_SFAULT_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Sfault_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Sfault_Enter_Icd => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_UCALL_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Ucall_Expt,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Ucall_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_UCALL_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Ucall_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Ucall_Enter_Icd => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_SCALL_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Scall_Expt,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Scall_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_SCALL_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Scall_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Scall_Enter_Icd => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_MCALL_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Mcall_Expt,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Mcall_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_MCALL_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Mcall_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Mcall_Enter_Icd => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_IPFAULT_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Ipfault_Expt,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Ipfault_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_IPFAULT_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Ipfault_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Ipfault_Enter_Icd => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_LPFAULT_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Lpfault_Expt,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Lpfault_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_LPFAULT_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Lpfault_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Lpfault_Enter_Icd => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_SPFAULT_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Spfault_Expt,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Spfault_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_SPFAULT_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Spfault_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_Spfault_Enter_Icd => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_U_SINGLE_STEP_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_U_Single_Step_Expt,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_U_Single_Step_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_U_SINGLE_STEP_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_U_Single_Step_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_U_Single_Step_Enter_Icd => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_S_SINGLE_STEP_FIELD is
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_S_Single_Step_Expt,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_S_Single_Step_Enter_Icd
    ) with size => 1;

    for LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_S_SINGLE_STEP_FIELD use
    (
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_S_Single_Step_Expt => 0,
        Lw_Priscv_Riscv_Icd_Rdata1_Rstat3_S_Single_Step_Enter_Icd => 1
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_ICD_RDATA1_REGISTER_0 is
    record
        F_Data    : LW_PRISCV_RISCV_ICD_RDATA1_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_ICD_RDATA1_REGISTER_0 use
    record
        F_Data at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
    type LW_PRISCV_RISCV_ICD_RDATA1_REGISTER_1 is
    record
        F_Rstat3_Iama    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_IAMA_FIELD;
        F_Rstat3_Ifault    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_IFAULT_FIELD;
        F_Rstat3_Ill    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_ILL_FIELD;
        F_Rstat3_Bkpt    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_BKPT_FIELD;
        F_Rstat3_Lama    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_LAMA_FIELD;
        F_Rstat3_Lfault    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_LFAULT_FIELD;
        F_Rstat3_Sama    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_SAMA_FIELD;
        F_Rstat3_Sfault    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_SFAULT_FIELD;
        F_Rstat3_Ucall    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_UCALL_FIELD;
        F_Rstat3_Scall    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_SCALL_FIELD;
        F_Rstat3_Mcall    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_MCALL_FIELD;
        F_Rstat3_Ipfault    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_IPFAULT_FIELD;
        F_Rstat3_Lpfault    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_LPFAULT_FIELD;
        F_Rstat3_Spfault    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_SPFAULT_FIELD;
        F_Rstat3_U_Single_Step    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_U_SINGLE_STEP_FIELD;
        F_Rstat3_S_Single_Step    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT3_S_SINGLE_STEP_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_ICD_RDATA1_REGISTER_1 use
    record
        F_Rstat3_Iama at 0 range 0 .. 0;
        F_Rstat3_Ifault at 0 range 1 .. 1;
        F_Rstat3_Ill at 0 range 2 .. 2;
        F_Rstat3_Bkpt at 0 range 3 .. 3;
        F_Rstat3_Lama at 0 range 4 .. 4;
        F_Rstat3_Lfault at 0 range 5 .. 5;
        F_Rstat3_Sama at 0 range 6 .. 6;
        F_Rstat3_Sfault at 0 range 7 .. 7;
        F_Rstat3_Ucall at 0 range 8 .. 8;
        F_Rstat3_Scall at 0 range 9 .. 9;
        F_Rstat3_Mcall at 0 range 11 .. 11;
        F_Rstat3_Ipfault at 0 range 12 .. 12;
        F_Rstat3_Lpfault at 0 range 13 .. 13;
        F_Rstat3_Spfault at 0 range 15 .. 15;
        F_Rstat3_U_Single_Step at 0 range 28 .. 28;
        F_Rstat3_S_Single_Step at 0 range 29 .. 29;
    end record;

------------------------------------------------------------------------------------------------------
    type LW_PRISCV_RISCV_ICD_RDATA1_REGISTER_2 is
    record
        F_Rstat4_Rs    : LW_PRISCV_RISCV_ICD_RDATA1_RSTAT4_RS_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_ICD_RDATA1_REGISTER_2 use
    record
        F_Rstat4_Rs at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Rpc    : constant Bar0_Addr := 16#0000_03EC#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_RPC_PC_LO_FIELD is new LwU32;
---------- Record Declaration ----------

    type LW_PRISCV_RISCV_RPC_REGISTER is
    record
        F_Pc_Lo    : LW_PRISCV_RISCV_RPC_PC_LO_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_RPC_REGISTER use
    record
        F_Pc_Lo at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Rstat0    : constant Bar0_Addr := 16#0000_03F0#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_RSTAT0_ROB_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_RSTAT0_IFU_INSTBUF_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_RSTAT0_IFU_LINEFILL_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_RSTAT0_LSU_STB_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_RSTAT0_LSU_MEM_WACK_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_RSTAT0_LSU_IO_WACK_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_RSTAT0_LSU_LINEFILL_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_RSTAT0_LSU_VICBUF_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_RSTAT0_LSU_CB_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_RSTAT0_ALU_RS_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_RSTAT0_IMD_RS_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_RSTAT0_FPU_RS_FULL_FIELD is new LwU1;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_RSTAT0_IOWR_INFLIGHT_FIELD is new LwU8;
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_RSTAT0_MEMWR_INFLIGHT_FIELD is new LwU8;
---------- Record Declaration ----------

    type LW_PRISCV_RISCV_RSTAT0_REGISTER is
    record
        F_Rob_Full    : LW_PRISCV_RISCV_RSTAT0_ROB_FULL_FIELD;
        F_Ifu_Instbuf_Full    : LW_PRISCV_RISCV_RSTAT0_IFU_INSTBUF_FULL_FIELD;
        F_Ifu_Linefill_Full    : LW_PRISCV_RISCV_RSTAT0_IFU_LINEFILL_FULL_FIELD;
        F_Lsu_Stb_Full    : LW_PRISCV_RISCV_RSTAT0_LSU_STB_FULL_FIELD;
        F_Lsu_Mem_Wack_Full    : LW_PRISCV_RISCV_RSTAT0_LSU_MEM_WACK_FULL_FIELD;
        F_Lsu_Io_Wack_Full    : LW_PRISCV_RISCV_RSTAT0_LSU_IO_WACK_FULL_FIELD;
        F_Lsu_Linefill_Full    : LW_PRISCV_RISCV_RSTAT0_LSU_LINEFILL_FULL_FIELD;
        F_Lsu_Vicbuf_Full    : LW_PRISCV_RISCV_RSTAT0_LSU_VICBUF_FULL_FIELD;
        F_Lsu_Cb_Full    : LW_PRISCV_RISCV_RSTAT0_LSU_CB_FULL_FIELD;
        F_Alu_Rs_Full    : LW_PRISCV_RISCV_RSTAT0_ALU_RS_FULL_FIELD;
        F_Imd_Rs_Full    : LW_PRISCV_RISCV_RSTAT0_IMD_RS_FULL_FIELD;
        F_Fpu_Rs_Full    : LW_PRISCV_RISCV_RSTAT0_FPU_RS_FULL_FIELD;
        F_Iowr_Inflight    : LW_PRISCV_RISCV_RSTAT0_IOWR_INFLIGHT_FIELD;
        F_Memwr_Inflight    : LW_PRISCV_RISCV_RSTAT0_MEMWR_INFLIGHT_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_RSTAT0_REGISTER use
    record
        F_Rob_Full at 0 range 0 .. 0;
        F_Ifu_Instbuf_Full at 0 range 1 .. 1;
        F_Ifu_Linefill_Full at 0 range 2 .. 2;
        F_Lsu_Stb_Full at 0 range 3 .. 3;
        F_Lsu_Mem_Wack_Full at 0 range 4 .. 4;
        F_Lsu_Io_Wack_Full at 0 range 5 .. 5;
        F_Lsu_Linefill_Full at 0 range 6 .. 6;
        F_Lsu_Vicbuf_Full at 0 range 7 .. 7;
        F_Lsu_Cb_Full at 0 range 8 .. 8;
        F_Alu_Rs_Full at 0 range 9 .. 9;
        F_Imd_Rs_Full at 0 range 10 .. 10;
        F_Fpu_Rs_Full at 0 range 11 .. 11;
        F_Iowr_Inflight at 0 range 16 .. 23;
        F_Memwr_Inflight at 0 range 24 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Tracectl    : constant Bar0_Addr := 16#0000_0400#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_TRACECTL_LOW_THSHD_FIELD is new LwU8;
    Lw_Priscv_Riscv_Tracectl_Low_Thshd_Init :
        constant LW_PRISCV_RISCV_TRACECTL_LOW_THSHD_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_TRACECTL_HIGH_THSHD_FIELD is new LwU8;
    Lw_Priscv_Riscv_Tracectl_High_Thshd_Init :
        constant LW_PRISCV_RISCV_TRACECTL_HIGH_THSHD_FIELD
        := 16#0000_00FF#;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_TRACECTL_UMODE_ENABLE_FIELD is
    (
        Lw_Priscv_Riscv_Tracectl_Umode_Enable_False,
        Lw_Priscv_Riscv_Tracectl_Umode_Enable_True
    ) with size => 1;

    for LW_PRISCV_RISCV_TRACECTL_UMODE_ENABLE_FIELD use
    (
        Lw_Priscv_Riscv_Tracectl_Umode_Enable_False => 0,
        Lw_Priscv_Riscv_Tracectl_Umode_Enable_True => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_TRACECTL_SMODE_ENABLE_FIELD is
    (
        Lw_Priscv_Riscv_Tracectl_Smode_Enable_False,
        Lw_Priscv_Riscv_Tracectl_Smode_Enable_True
    ) with size => 1;

    for LW_PRISCV_RISCV_TRACECTL_SMODE_ENABLE_FIELD use
    (
        Lw_Priscv_Riscv_Tracectl_Smode_Enable_False => 0,
        Lw_Priscv_Riscv_Tracectl_Smode_Enable_True => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_TRACECTL_MMODE_ENABLE_FIELD is
    (
        Lw_Priscv_Riscv_Tracectl_Mmode_Enable_False,
        Lw_Priscv_Riscv_Tracectl_Mmode_Enable_True
    ) with size => 1;

    for LW_PRISCV_RISCV_TRACECTL_MMODE_ENABLE_FIELD use
    (
        Lw_Priscv_Riscv_Tracectl_Mmode_Enable_False => 0,
        Lw_Priscv_Riscv_Tracectl_Mmode_Enable_True => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_TRACECTL_MODE_FIELD is
    (
        Lw_Priscv_Riscv_Tracectl_Mode_Full,
        Lw_Priscv_Riscv_Tracectl_Mode_Reduced,
        Lw_Priscv_Riscv_Tracectl_Mode_Stack
    ) with size => 2;

    for LW_PRISCV_RISCV_TRACECTL_MODE_FIELD use
    (
        Lw_Priscv_Riscv_Tracectl_Mode_Full => 0,
        Lw_Priscv_Riscv_Tracectl_Mode_Reduced => 1,
        Lw_Priscv_Riscv_Tracectl_Mode_Stack => 2
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_TRACECTL_BELOW_LO_FIELD is new LwU1;
    Lw_Priscv_Riscv_Tracectl_Below_Lo_Init :
        constant LW_PRISCV_RISCV_TRACECTL_BELOW_LO_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_TRACECTL_ABOVE_HI_FIELD is new LwU1;
    Lw_Priscv_Riscv_Tracectl_Above_Hi_Init :
        constant LW_PRISCV_RISCV_TRACECTL_ABOVE_HI_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_TRACECTL_INTR_ENABLE_FIELD is
    (
        Lw_Priscv_Riscv_Tracectl_Intr_Enable_False,
        Lw_Priscv_Riscv_Tracectl_Intr_Enable_True
    ) with size => 1;

    for LW_PRISCV_RISCV_TRACECTL_INTR_ENABLE_FIELD use
    (
        Lw_Priscv_Riscv_Tracectl_Intr_Enable_False => 0,
        Lw_Priscv_Riscv_Tracectl_Intr_Enable_True => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_TRACECTL_FULL_FIELD is new LwU1;
    Lw_Priscv_Riscv_Tracectl_Full_Init :
        constant LW_PRISCV_RISCV_TRACECTL_FULL_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_TRACECTL_EMPTY_FIELD is new LwU1;
    Lw_Priscv_Riscv_Tracectl_Empty_Init :
        constant LW_PRISCV_RISCV_TRACECTL_EMPTY_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_TRACECTL_REGISTER is
    record
        F_Low_Thshd    : LW_PRISCV_RISCV_TRACECTL_LOW_THSHD_FIELD;
        F_High_Thshd    : LW_PRISCV_RISCV_TRACECTL_HIGH_THSHD_FIELD;
        F_Umode_Enable    : LW_PRISCV_RISCV_TRACECTL_UMODE_ENABLE_FIELD;
        F_Smode_Enable    : LW_PRISCV_RISCV_TRACECTL_SMODE_ENABLE_FIELD;
        F_Mmode_Enable    : LW_PRISCV_RISCV_TRACECTL_MMODE_ENABLE_FIELD;
        F_Mode    : LW_PRISCV_RISCV_TRACECTL_MODE_FIELD;
        F_Below_Lo    : LW_PRISCV_RISCV_TRACECTL_BELOW_LO_FIELD;
        F_Above_Hi    : LW_PRISCV_RISCV_TRACECTL_ABOVE_HI_FIELD;
        F_Intr_Enable    : LW_PRISCV_RISCV_TRACECTL_INTR_ENABLE_FIELD;
        F_Full    : LW_PRISCV_RISCV_TRACECTL_FULL_FIELD;
        F_Empty    : LW_PRISCV_RISCV_TRACECTL_EMPTY_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_TRACECTL_REGISTER use
    record
        F_Low_Thshd at 0 range 0 .. 7;
        F_High_Thshd at 0 range 8 .. 15;
        F_Umode_Enable at 0 range 20 .. 20;
        F_Smode_Enable at 0 range 21 .. 21;
        F_Mmode_Enable at 0 range 23 .. 23;
        F_Mode at 0 range 24 .. 25;
        F_Below_Lo at 0 range 27 .. 27;
        F_Above_Hi at 0 range 28 .. 28;
        F_Intr_Enable at 0 range 29 .. 29;
        F_Full at 0 range 30 .. 30;
        F_Empty at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Trace_Rdidx    : constant Bar0_Addr := 16#0000_0404#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_TRACE_RDIDX_RDIDX_FIELD is new LwU8;
    Lw_Priscv_Riscv_Trace_Rdidx_Rdidx_Init :
        constant LW_PRISCV_RISCV_TRACE_RDIDX_RDIDX_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_TRACE_RDIDX_RSVD0_FIELD is new LwU8;
    Lw_Priscv_Riscv_Trace_Rdidx_Rsvd0_Init :
        constant LW_PRISCV_RISCV_TRACE_RDIDX_RSVD0_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_TRACE_RDIDX_MAXIDX_FIELD is new LwU8;
    Lw_Priscv_Riscv_Trace_Rdidx_Maxidx_Init :
        constant LW_PRISCV_RISCV_TRACE_RDIDX_MAXIDX_FIELD
        := 16#0000_003F#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_TRACE_RDIDX_RSVD1_FIELD is new LwU8;
    Lw_Priscv_Riscv_Trace_Rdidx_Rsvd1_Init :
        constant LW_PRISCV_RISCV_TRACE_RDIDX_RSVD1_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_TRACE_RDIDX_REGISTER is
    record
        F_Rdidx    : LW_PRISCV_RISCV_TRACE_RDIDX_RDIDX_FIELD;
        F_Rsvd0    : LW_PRISCV_RISCV_TRACE_RDIDX_RSVD0_FIELD;
        F_Maxidx    : LW_PRISCV_RISCV_TRACE_RDIDX_MAXIDX_FIELD;
        F_Rsvd1    : LW_PRISCV_RISCV_TRACE_RDIDX_RSVD1_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_TRACE_RDIDX_REGISTER use
    record
        F_Rdidx at 0 range 0 .. 7;
        F_Rsvd0 at 0 range 8 .. 15;
        F_Maxidx at 0 range 16 .. 23;
        F_Rsvd1 at 0 range 24 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Trace_Wtidx    : constant Bar0_Addr := 16#0000_0408#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_TRACE_WTIDX_RSVD1_FIELD is new LwU24;
    Lw_Priscv_Riscv_Trace_Wtidx_Rsvd1_Init :
        constant LW_PRISCV_RISCV_TRACE_WTIDX_RSVD1_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_TRACE_WTIDX_WTIDX_FIELD is new LwU8;
    Lw_Priscv_Riscv_Trace_Wtidx_Wtidx_Init :
        constant LW_PRISCV_RISCV_TRACE_WTIDX_WTIDX_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_TRACE_WTIDX_REGISTER is
    record
        F_Rsvd1    : LW_PRISCV_RISCV_TRACE_WTIDX_RSVD1_FIELD;
        F_Wtidx    : LW_PRISCV_RISCV_TRACE_WTIDX_WTIDX_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_TRACE_WTIDX_REGISTER use
    record
        F_Rsvd1 at 0 range 0 .. 23;
        F_Wtidx at 0 range 24 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Tracepc_Hi    : constant Bar0_Addr := 16#0000_0410#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_TRACEPC_HI_PC_FIELD is new LwU32;
    Lw_Priscv_Riscv_Tracepc_Hi_Pc_Init :
        constant LW_PRISCV_RISCV_TRACEPC_HI_PC_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_TRACEPC_HI_REGISTER is
    record
        F_Pc    : LW_PRISCV_RISCV_TRACEPC_HI_PC_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_TRACEPC_HI_REGISTER use
    record
        F_Pc at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Tracepc_Lo    : constant Bar0_Addr := 16#0000_040C#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_TRACEPC_LO_PC_FIELD is new LwU32;
    Lw_Priscv_Riscv_Tracepc_Lo_Pc_Init :
        constant LW_PRISCV_RISCV_TRACEPC_LO_PC_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_TRACEPC_LO_REGISTER is
    record
        F_Pc    : LW_PRISCV_RISCV_TRACEPC_LO_PC_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_TRACEPC_LO_REGISTER use
    record
        F_Pc at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Mtimecmp_Hi    : constant Bar0_Addr := 16#0000_03C4#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_MTIMECMP_HI_VALUE_FIELD is new LwU32;
    Lw_Priscv_Riscv_Mtimecmp_Hi_Value_Init :
        constant LW_PRISCV_RISCV_MTIMECMP_HI_VALUE_FIELD
        := 16#FFFF_FFFF#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_MTIMECMP_HI_REGISTER is
    record
        F_Value    : LW_PRISCV_RISCV_MTIMECMP_HI_VALUE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_MTIMECMP_HI_REGISTER use
    record
        F_Value at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Mtimecmp_Lo    : constant Bar0_Addr := 16#0000_03C0#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_MTIMECMP_LO_VALUE_FIELD is new LwU32;
    Lw_Priscv_Riscv_Mtimecmp_Lo_Value_Init :
        constant LW_PRISCV_RISCV_MTIMECMP_LO_VALUE_FIELD
        := 16#FFFF_FFFF#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_MTIMECMP_LO_REGISTER is
    record
        F_Value    : LW_PRISCV_RISCV_MTIMECMP_LO_VALUE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_MTIMECMP_LO_REGISTER use
    record
        F_Value at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Intr_Status    : constant Bar0_Addr := 16#0000_0394#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_INTR_STATUS_TIMER_INTR_FIELD is new LwU1;
    Lw_Priscv_Riscv_Intr_Status_Timer_Intr_Init :
        constant LW_PRISCV_RISCV_INTR_STATUS_TIMER_INTR_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_INTR_STATUS_EXTERNAL_INTR_FIELD is new LwU1;
    Lw_Priscv_Riscv_Intr_Status_External_Intr_Int :
        constant LW_PRISCV_RISCV_INTR_STATUS_EXTERNAL_INTR_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_INTR_STATUS_SOFTWARE_INTR_FIELD is new LwU1;
    Lw_Priscv_Riscv_Intr_Status_Software_Intr_Init :
        constant LW_PRISCV_RISCV_INTR_STATUS_SOFTWARE_INTR_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_INTR_STATUS_S_TIMER_INTR_FIELD is new LwU1;
    Lw_Priscv_Riscv_Intr_Status_S_Timer_Intr_Init :
        constant LW_PRISCV_RISCV_INTR_STATUS_S_TIMER_INTR_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_INTR_STATUS_S_EXTERNAL_INTR_FIELD is new LwU1;
    Lw_Priscv_Riscv_Intr_Status_S_External_Intr_Int :
        constant LW_PRISCV_RISCV_INTR_STATUS_S_EXTERNAL_INTR_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_INTR_STATUS_S_SOFTWARE_INTR_FIELD is new LwU1;
    Lw_Priscv_Riscv_Intr_Status_S_Software_Intr_Init :
        constant LW_PRISCV_RISCV_INTR_STATUS_S_SOFTWARE_INTR_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_INTR_STATUS_REGISTER is
    record
        F_Timer_Intr    : LW_PRISCV_RISCV_INTR_STATUS_TIMER_INTR_FIELD;
        F_External_Intr    : LW_PRISCV_RISCV_INTR_STATUS_EXTERNAL_INTR_FIELD;
        F_Software_Intr    : LW_PRISCV_RISCV_INTR_STATUS_SOFTWARE_INTR_FIELD;
        F_S_Timer_Intr    : LW_PRISCV_RISCV_INTR_STATUS_S_TIMER_INTR_FIELD;
        F_S_External_Intr    : LW_PRISCV_RISCV_INTR_STATUS_S_EXTERNAL_INTR_FIELD;
        F_S_Software_Intr    : LW_PRISCV_RISCV_INTR_STATUS_S_SOFTWARE_INTR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_INTR_STATUS_REGISTER use
    record
        F_Timer_Intr at 0 range 0 .. 0;
        F_External_Intr at 0 range 1 .. 1;
        F_Software_Intr at 0 range 2 .. 2;
        F_S_Timer_Intr at 0 range 3 .. 3;
        F_S_External_Intr at 0 range 4 .. 4;
        F_S_Software_Intr at 0 range 5 .. 5;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Msip    : constant Bar0_Addr := 16#0000_038C#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_MSIP_VLD_FIELD is
    (
        Lw_Priscv_Riscv_Msip_Vld_False,
        Lw_Priscv_Riscv_Msip_Vld_True
    ) with size => 1;

    for LW_PRISCV_RISCV_MSIP_VLD_FIELD use
    (
        Lw_Priscv_Riscv_Msip_Vld_False => 0,
        Lw_Priscv_Riscv_Msip_Vld_True => 1
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_MSIP_REGISTER is
    record
        F_Vld    : LW_PRISCV_RISCV_MSIP_VLD_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_MSIP_REGISTER use
    record
        F_Vld at 0 range 0 .. 0;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Priv_Err_Stat    : constant Bar0_Addr := 16#0000_0500#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_PRIV_ERR_STAT_RESERVED_FIELD is new LwU31;
    Lw_Priscv_Riscv_Priv_Err_Stat_Reserved_Init :
        constant LW_PRISCV_RISCV_PRIV_ERR_STAT_RESERVED_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_PRIV_ERR_STAT_VALID_FIELD is
    (
        Lw_Priscv_Riscv_Priv_Err_Stat_Valid_False,
        Lw_Priscv_Riscv_Priv_Err_Stat_Valid_True
    ) with size => 1;

    for LW_PRISCV_RISCV_PRIV_ERR_STAT_VALID_FIELD use
    (
        Lw_Priscv_Riscv_Priv_Err_Stat_Valid_False => 0,
        Lw_Priscv_Riscv_Priv_Err_Stat_Valid_True => 1
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_PRIV_ERR_STAT_REGISTER is
    record
        F_Reserved    : LW_PRISCV_RISCV_PRIV_ERR_STAT_RESERVED_FIELD;
        F_Valid    : LW_PRISCV_RISCV_PRIV_ERR_STAT_VALID_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_PRIV_ERR_STAT_REGISTER use
    record
        F_Reserved at 0 range 0 .. 30;
        F_Valid at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Priv_Err_Info    : constant Bar0_Addr := 16#0000_0504#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_PRIV_ERR_INFO_ERROR_INFO_FIELD is new LwU32;
    Lw_Priscv_Riscv_Priv_Err_Info_Error_Info_Init :
        constant LW_PRISCV_RISCV_PRIV_ERR_INFO_ERROR_INFO_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_PRIV_ERR_INFO_REGISTER is
    record
        F_Error_Info    : LW_PRISCV_RISCV_PRIV_ERR_INFO_ERROR_INFO_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_PRIV_ERR_INFO_REGISTER use
    record
        F_Error_Info at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Priv_Err_Addr    : constant Bar0_Addr := 16#0000_0508#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_PRIV_ERR_ADDR_ADDR_FIELD is new LwU32;
    Lw_Priscv_Riscv_Priv_Err_Addr_Addr_Init :
        constant LW_PRISCV_RISCV_PRIV_ERR_ADDR_ADDR_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_PRIV_ERR_ADDR_REGISTER is
    record
        F_Addr    : LW_PRISCV_RISCV_PRIV_ERR_ADDR_ADDR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_PRIV_ERR_ADDR_REGISTER use
    record
        F_Addr at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Priv_Err_Addr_Hi    : constant Bar0_Addr := 16#0000_050C#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_PRIV_ERR_ADDR_HI_ADDR_FIELD is new LwU32;
    Lw_Priscv_Riscv_Priv_Err_Addr_Hi_Addr_Init :
        constant LW_PRISCV_RISCV_PRIV_ERR_ADDR_HI_ADDR_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_PRIV_ERR_ADDR_HI_REGISTER is
    record
        F_Addr    : LW_PRISCV_RISCV_PRIV_ERR_ADDR_HI_ADDR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_PRIV_ERR_ADDR_HI_REGISTER use
    record
        F_Addr at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Hub_Err_Stat    : constant Bar0_Addr := 16#0000_0510#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_HUB_ERR_STAT_RESERVED_FIELD is new LwU31;
    Lw_Priscv_Riscv_Hub_Err_Stat_Reserved_Init :
        constant LW_PRISCV_RISCV_HUB_ERR_STAT_RESERVED_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_HUB_ERR_STAT_VALID_FIELD is
    (
        Lw_Priscv_Riscv_Hub_Err_Stat_Valid_False,
        Lw_Priscv_Riscv_Hub_Err_Stat_Valid_True
    ) with size => 1;

    for LW_PRISCV_RISCV_HUB_ERR_STAT_VALID_FIELD use
    (
        Lw_Priscv_Riscv_Hub_Err_Stat_Valid_False => 0,
        Lw_Priscv_Riscv_Hub_Err_Stat_Valid_True => 1
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_HUB_ERR_STAT_REGISTER is
    record
        F_Reserved    : LW_PRISCV_RISCV_HUB_ERR_STAT_RESERVED_FIELD;
        F_Valid    : LW_PRISCV_RISCV_HUB_ERR_STAT_VALID_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_HUB_ERR_STAT_REGISTER use
    record
        F_Reserved at 0 range 0 .. 30;
        F_Valid at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Irom_Patch_Cam_Index    : constant Bar0_Addr := 16#0000_0590#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IROM_PATCH_CAM_INDEX_INDEX_FIELD is new LwU5;
    Lw_Priscv_Riscv_Irom_Patch_Cam_Index_Index_Init :
        constant LW_PRISCV_RISCV_IROM_PATCH_CAM_INDEX_INDEX_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IROM_PATCH_CAM_INDEX_REGISTER is
    record
        F_Index    : LW_PRISCV_RISCV_IROM_PATCH_CAM_INDEX_INDEX_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IROM_PATCH_CAM_INDEX_REGISTER use
    record
        F_Index at 0 range 0 .. 4;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Irom_Patch_Cam_Addr_Valid    : constant Bar0_Addr := 16#0000_0594#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IROM_PATCH_CAM_ADDR_VALID_ADDR_FIELD is new LwU18;
    Lw_Priscv_Riscv_Irom_Patch_Cam_Addr_Valid_Addr_Init :
        constant LW_PRISCV_RISCV_IROM_PATCH_CAM_ADDR_VALID_ADDR_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IROM_PATCH_CAM_ADDR_VALID_VALID_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irom_Patch_Cam_Addr_Valid_Valid_Init :
        constant LW_PRISCV_RISCV_IROM_PATCH_CAM_ADDR_VALID_VALID_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IROM_PATCH_CAM_ADDR_VALID_REGISTER is
    record
        F_Addr    : LW_PRISCV_RISCV_IROM_PATCH_CAM_ADDR_VALID_ADDR_FIELD;
        F_Valid    : LW_PRISCV_RISCV_IROM_PATCH_CAM_ADDR_VALID_VALID_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IROM_PATCH_CAM_ADDR_VALID_REGISTER use
    record
        F_Addr at 0 range 0 .. 17;
        F_Valid at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Irom_Patch_Cam_Data    : constant Bar0_Addr := 16#0000_0598#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IROM_PATCH_CAM_DATA_DATA_FIELD is new LwU16;
    Lw_Priscv_Riscv_Irom_Patch_Cam_Data_Data_Init :
        constant LW_PRISCV_RISCV_IROM_PATCH_CAM_DATA_DATA_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IROM_PATCH_CAM_DATA_REGISTER is
    record
        F_Data    : LW_PRISCV_RISCV_IROM_PATCH_CAM_DATA_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IROM_PATCH_CAM_DATA_REGISTER use
    record
        F_Data at 0 range 0 .. 15;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Irom_Patch_Selwre    : constant Bar0_Addr := 16#0000_059C#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IROM_PATCH_SELWRE_IPATCH_LOCK_FIELD is new LwU1;
    Lw_Priscv_Riscv_Irom_Patch_Selwre_Ipatch_Lock_Init :
        constant LW_PRISCV_RISCV_IROM_PATCH_SELWRE_IPATCH_LOCK_FIELD
        := 16#0000_0001#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IROM_PATCH_SELWRE_REGISTER is
    record
        F_Ipatch_Lock    : LW_PRISCV_RISCV_IROM_PATCH_SELWRE_IPATCH_LOCK_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IROM_PATCH_SELWRE_REGISTER use
    record
        F_Ipatch_Lock at 0 range 0 .. 0;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Iopmp_Index    : constant Bar0_Addr := 16#0000_05B0#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_INDEX_VAL_FIELD is new LwU5;
    Lw_Priscv_Riscv_Iopmp_Index_Val_Init :
        constant LW_PRISCV_RISCV_IOPMP_INDEX_VAL_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_INDEX_REGISTER is
    record
        F_Val    : LW_PRISCV_RISCV_IOPMP_INDEX_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IOPMP_INDEX_REGISTER use
    record
        F_Val at 0 range 0 .. 4;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Iopmp_Cfg    : constant Bar0_Addr := 16#0000_05B4#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_CFG_READ_FIELD is
    (
        Lw_Priscv_Riscv_Iopmp_Cfg_Read_Disable,
        Lw_Priscv_Riscv_Iopmp_Cfg_Read_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_IOPMP_CFG_READ_FIELD use
    (
        Lw_Priscv_Riscv_Iopmp_Cfg_Read_Disable => 0,
        Lw_Priscv_Riscv_Iopmp_Cfg_Read_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_CFG_WRITE_FIELD is
    (
        Lw_Priscv_Riscv_Iopmp_Cfg_Write_Disable,
        Lw_Priscv_Riscv_Iopmp_Cfg_Write_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_IOPMP_CFG_WRITE_FIELD use
    (
        Lw_Priscv_Riscv_Iopmp_Cfg_Write_Disable => 0,
        Lw_Priscv_Riscv_Iopmp_Cfg_Write_Enable => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_CFG_MASTER_FIELD is new LwU12;
    Lw_Priscv_Riscv_Iopmp_Cfg_Master_All_Masters_Enabled :
        constant LW_PRISCV_RISCV_IOPMP_CFG_MASTER_FIELD
        := 16#0000_0FF7#;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_CFG_LOCK_FIELD is
    (
        Lw_Priscv_Riscv_Iopmp_Cfg_Lock_Unlocked,
        Lw_Priscv_Riscv_Iopmp_Cfg_Lock_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_IOPMP_CFG_LOCK_FIELD use
    (
        Lw_Priscv_Riscv_Iopmp_Cfg_Lock_Unlocked => 0,
        Lw_Priscv_Riscv_Iopmp_Cfg_Lock_Locked => 1
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_CFG_REGISTER is
    record
        F_Read    : LW_PRISCV_RISCV_IOPMP_CFG_READ_FIELD;
        F_Write    : LW_PRISCV_RISCV_IOPMP_CFG_WRITE_FIELD;
        F_Master    : LW_PRISCV_RISCV_IOPMP_CFG_MASTER_FIELD;
        F_Lock    : LW_PRISCV_RISCV_IOPMP_CFG_LOCK_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IOPMP_CFG_REGISTER use
    record
        F_Read at 0 range 0 .. 0;
        F_Write at 0 range 1 .. 1;
        F_Master at 0 range 4 .. 15;
        F_Lock at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Iopmp_Addr_Hi    : constant Bar0_Addr := 16#0000_05BC#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ADDR_HI_VAL_FIELD is new LwU29;
    Lw_Priscv_Riscv_Iopmp_Addr_Hi_Val_Init :
        constant LW_PRISCV_RISCV_IOPMP_ADDR_HI_VAL_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ADDR_HI_REGISTER is
    record
        F_Val    : LW_PRISCV_RISCV_IOPMP_ADDR_HI_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IOPMP_ADDR_HI_REGISTER use
    record
        F_Val at 0 range 0 .. 28;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Iopmp_Addr_Lo    : constant Bar0_Addr := 16#0000_05B8#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ADDR_LO_1K_FIELD is new LwU7;
    Lw_Priscv_Riscv_Iopmp_Addr_Lo_1K_Init :
        constant LW_PRISCV_RISCV_IOPMP_ADDR_LO_1K_FIELD
        := 16#0000_007F#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ADDR_LO_ABOVE1K_FIELD is new LwU25;
    Lw_Priscv_Riscv_Iopmp_Addr_Lo_Above1K_Init :
        constant LW_PRISCV_RISCV_IOPMP_ADDR_LO_ABOVE1K_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ADDR_LO_REGISTER is
    record
        F_1K    : LW_PRISCV_RISCV_IOPMP_ADDR_LO_1K_FIELD;
        F_Above1K    : LW_PRISCV_RISCV_IOPMP_ADDR_LO_ABOVE1K_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IOPMP_ADDR_LO_REGISTER use
    record
        F_1K at 0 range 0 .. 6;
        F_Above1K at 0 range 7 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Iopmp_Mode    : constant Bar0_Addr := 16#0000_05C0#;

   LW_PRISCV_RISCV_IOPMP_MODE_SIZE_1 : constant := 16;
   LW_PRISCV_RISCV_IOPMP_MODE_MAX_INDEX : constant := LW_PRISCV_RISCV_IOPMP_MODE_SIZE_1 - 1;
   subtype LW_PRISCV_RISCV_IOPMP_MODE_INDEX is LwU32 range 0 .. LW_PRISCV_RISCV_IOPMP_MODE_MAX_INDEX;

   function Lw_Priscv_Riscv_Iopmp_Mode_Addr( Index : LW_PRISCV_RISCV_IOPMP_MODE_INDEX )
                                            return Bar0_Addr is
     (Bar0_Addr(LwU32(Lw_Priscv_Riscv_Iopmp_Mode) + (LwU32(Index)*4)))
      with Inline_Always,
     Global => null,
     Depends => ( Lw_Priscv_Riscv_Iopmp_Mode_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_MODE_VAL_FIELD is new LwU32;
    Lw_Priscv_Riscv_Iopmp_Mode_Val_Init :
        constant LW_PRISCV_RISCV_IOPMP_MODE_VAL_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_MODE_REGISTER is
    record
        F_Val    : LW_PRISCV_RISCV_IOPMP_MODE_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IOPMP_MODE_REGISTER use
    record
        F_Val at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Iopmp_Err_Stat    : constant Bar0_Addr := 16#0000_05E0#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ERR_STAT_VALID_FIELD is
    (
        Lw_Priscv_Riscv_Iopmp_Err_Stat_Valid_False,
        Lw_Priscv_Riscv_Iopmp_Err_Stat_Valid_Ture
    ) with size => 1;

    for LW_PRISCV_RISCV_IOPMP_ERR_STAT_VALID_FIELD use
    (
        Lw_Priscv_Riscv_Iopmp_Err_Stat_Valid_False => 0,
        Lw_Priscv_Riscv_Iopmp_Err_Stat_Valid_Ture => 1
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ERR_STAT_REGISTER is
    record
        F_Valid    : LW_PRISCV_RISCV_IOPMP_ERR_STAT_VALID_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IOPMP_ERR_STAT_REGISTER use
    record
        F_Valid at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Iopmp_Err_Info    : constant Bar0_Addr := 16#0000_05E4#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ERR_INFO_MASTER_FIELD is
    (
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Fbdma,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Cpdma,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Sha,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Pmb0,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Pmb1,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Pmb2,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Pmb3,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Pmb4,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Pmb5,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Pmb6,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Pmb7
    ) with size => 4;

    for LW_PRISCV_RISCV_IOPMP_ERR_INFO_MASTER_FIELD use
    (
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Fbdma => 0,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Cpdma => 1,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Sha => 2,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Pmb0 => 4,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Pmb1 => 5,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Pmb2 => 6,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Pmb3 => 7,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Pmb4 => 8,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Pmb5 => 9,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Pmb6 => 10,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Master_Pmb7 => 11
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ERR_INFO_READ_FIELD is
    (
        Lw_Priscv_Riscv_Iopmp_Err_Info_Read_False,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Read_True
    ) with size => 1;

    for LW_PRISCV_RISCV_IOPMP_ERR_INFO_READ_FIELD use
    (
        Lw_Priscv_Riscv_Iopmp_Err_Info_Read_False => 0,
        Lw_Priscv_Riscv_Iopmp_Err_Info_Read_True => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ERR_INFO_ENTRY_FIELD is new LwU6;
    Lw_Priscv_Riscv_Iopmp_Err_Info_Entry_Init :
        constant LW_PRISCV_RISCV_IOPMP_ERR_INFO_ENTRY_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ERR_INFO_REGISTER is
    record
        F_Master    : LW_PRISCV_RISCV_IOPMP_ERR_INFO_MASTER_FIELD;
        F_Read    : LW_PRISCV_RISCV_IOPMP_ERR_INFO_READ_FIELD;
        F_Entry    : LW_PRISCV_RISCV_IOPMP_ERR_INFO_ENTRY_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IOPMP_ERR_INFO_REGISTER use
    record
        F_Master at 0 range 0 .. 3;
        F_Read at 0 range 4 .. 4;
        F_Entry at 0 range 16 .. 21;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Iopmp_Err_Addr_Hi    : constant Bar0_Addr := 16#0000_05EC#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ERR_ADDR_HI_ADDR_FIELD is new LwU32;
    Lw_Priscv_Riscv_Iopmp_Err_Addr_Hi_Addr_Init :
        constant LW_PRISCV_RISCV_IOPMP_ERR_ADDR_HI_ADDR_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ERR_ADDR_HI_REGISTER is
    record
        F_Addr    : LW_PRISCV_RISCV_IOPMP_ERR_ADDR_HI_ADDR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IOPMP_ERR_ADDR_HI_REGISTER use
    record
        F_Addr at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Iopmp_Err_Addr_Lo    : constant Bar0_Addr := 16#0000_05E8#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ERR_ADDR_LO_ADDR_FIELD is new LwU32;
    Lw_Priscv_Riscv_Iopmp_Err_Addr_Lo_Addr_Init :
        constant LW_PRISCV_RISCV_IOPMP_ERR_ADDR_LO_ADDR_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ERR_ADDR_LO_REGISTER is
    record
        F_Addr    : LW_PRISCV_RISCV_IOPMP_ERR_ADDR_LO_ADDR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IOPMP_ERR_ADDR_LO_REGISTER use
    record
        F_Addr at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Iopmp_Err_Capen    : constant Bar0_Addr := 16#0000_05F0#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ERR_CAPEN_MASTER_FIELD is new LwU12;
    Lw_Priscv_Riscv_Iopmp_Err_Capen_Master_All_Masters_Enabled :
        constant LW_PRISCV_RISCV_IOPMP_ERR_CAPEN_MASTER_FIELD
        := 16#0000_0FFF#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_IOPMP_ERR_CAPEN_REGISTER is
    record
        F_Master    : LW_PRISCV_RISCV_IOPMP_ERR_CAPEN_MASTER_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_IOPMP_ERR_CAPEN_REGISTER use
    record
        F_Master at 0 range 0 .. 11;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Scpdmatrfmoffs    : constant Bar0_Addr := 16#0000_0570#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_SCPDMATRFMOFFS_OFFS_FIELD is new LwU32;
    Lw_Priscv_Riscv_Scpdmatrfmoffs_Offs_Init :
        constant LW_PRISCV_RISCV_SCPDMATRFMOFFS_OFFS_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_SCPDMATRFMOFFS_REGISTER is
    record
        F_Offs    : LW_PRISCV_RISCV_SCPDMATRFMOFFS_OFFS_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_SCPDMATRFMOFFS_REGISTER use
    record
        F_Offs at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Scpdmatrfcmd    : constant Bar0_Addr := 16#0000_0574#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_SCPDMATRFCMD_SUPPRESS_FIELD is
    (
        Lw_Priscv_Riscv_Scpdmatrfcmd_Suppress_Disable,
        Lw_Priscv_Riscv_Scpdmatrfcmd_Suppress_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_SCPDMATRFCMD_SUPPRESS_FIELD use
    (
        Lw_Priscv_Riscv_Scpdmatrfcmd_Suppress_Disable => 0,
        Lw_Priscv_Riscv_Scpdmatrfcmd_Suppress_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_SCPDMATRFCMD_SHORTLWT_FIELD is
    (
        Lw_Priscv_Riscv_Scpdmatrfcmd_Shortlwt_Disable,
        Lw_Priscv_Riscv_Scpdmatrfcmd_Shortlwt_Enable
    ) with size => 1;

    for LW_PRISCV_RISCV_SCPDMATRFCMD_SHORTLWT_FIELD use
    (
        Lw_Priscv_Riscv_Scpdmatrfcmd_Shortlwt_Disable => 0,
        Lw_Priscv_Riscv_Scpdmatrfcmd_Shortlwt_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_SCPDMATRFCMD_IMEM_FIELD is
    (
        Lw_Priscv_Riscv_Scpdmatrfcmd_Imem_False,
        Lw_Priscv_Riscv_Scpdmatrfcmd_Imem_True
    ) with size => 1;

    for LW_PRISCV_RISCV_SCPDMATRFCMD_IMEM_FIELD use
    (
        Lw_Priscv_Riscv_Scpdmatrfcmd_Imem_False => 0,
        Lw_Priscv_Riscv_Scpdmatrfcmd_Imem_True => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_SCPDMATRFCMD_WRITE_FIELD is
    (
        Lw_Priscv_Riscv_Scpdmatrfcmd_Write_False,
        Lw_Priscv_Riscv_Scpdmatrfcmd_Write_True
    ) with size => 1;

    for LW_PRISCV_RISCV_SCPDMATRFCMD_WRITE_FIELD use
    (
        Lw_Priscv_Riscv_Scpdmatrfcmd_Write_False => 0,
        Lw_Priscv_Riscv_Scpdmatrfcmd_Write_True => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_SCPDMATRFCMD_GPR_FIELD is new LwU3;
    Lw_Priscv_Riscv_Scpdmatrfcmd_Gpr_Init :
        constant LW_PRISCV_RISCV_SCPDMATRFCMD_GPR_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_SCPDMATRFCMD_CCI_EX_FIELD is
    (
        Lw_Priscv_Riscv_Scpdmatrfcmd_Cci_Ex_Scpdma,
        Lw_Priscv_Riscv_Scpdmatrfcmd_Cci_Ex_Cci
    ) with size => 1;

    for LW_PRISCV_RISCV_SCPDMATRFCMD_CCI_EX_FIELD use
    (
        Lw_Priscv_Riscv_Scpdmatrfcmd_Cci_Ex_Scpdma => 0,
        Lw_Priscv_Riscv_Scpdmatrfcmd_Cci_Ex_Cci => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_SCPDMATRFCMD_CCI_CMD_FIELD is new LwU16;
    Lw_Priscv_Riscv_Scpdmatrfcmd_Cci_Cmd_Init :
        constant LW_PRISCV_RISCV_SCPDMATRFCMD_CCI_CMD_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_SCPDMATRFCMD_REGISTER is
    record
        F_Suppress    : LW_PRISCV_RISCV_SCPDMATRFCMD_SUPPRESS_FIELD;
        F_Shortlwt    : LW_PRISCV_RISCV_SCPDMATRFCMD_SHORTLWT_FIELD;
        F_Imem    : LW_PRISCV_RISCV_SCPDMATRFCMD_IMEM_FIELD;
        F_Write    : LW_PRISCV_RISCV_SCPDMATRFCMD_WRITE_FIELD;
        F_Gpr    : LW_PRISCV_RISCV_SCPDMATRFCMD_GPR_FIELD;
        F_Cci_Ex    : LW_PRISCV_RISCV_SCPDMATRFCMD_CCI_EX_FIELD;
        F_Cci_Cmd    : LW_PRISCV_RISCV_SCPDMATRFCMD_CCI_CMD_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_SCPDMATRFCMD_REGISTER use
    record
        F_Suppress at 0 range 2 .. 2;
        F_Shortlwt at 0 range 3 .. 3;
        F_Imem at 0 range 4 .. 4;
        F_Write at 0 range 5 .. 5;
        F_Gpr at 0 range 6 .. 8;
        F_Cci_Ex at 0 range 15 .. 15;
        F_Cci_Cmd at 0 range 16 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Scpdmapoll    : constant Bar0_Addr := 16#0000_0578#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_SCPDMAPOLL_DMA_ACTIVE_FIELD is
    (
        Lw_Priscv_Riscv_Scpdmapoll_Dma_Active_Idle,
        Lw_Priscv_Riscv_Scpdmapoll_Dma_Active_Active
    ) with size => 1;

    for LW_PRISCV_RISCV_SCPDMAPOLL_DMA_ACTIVE_FIELD use
    (
        Lw_Priscv_Riscv_Scpdmapoll_Dma_Active_Idle => 0,
        Lw_Priscv_Riscv_Scpdmapoll_Dma_Active_Active => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_SCPDMAPOLL_ERROR_CLR_FIELD is new LwU1;
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_SCPDMAPOLL_ERROR_CODE_FIELD is
    (
        Lw_Priscv_Riscv_Scpdmapoll_Error_Code_No_Error,
        Lw_Priscv_Riscv_Scpdmapoll_Error_Code_Not_Size_Aligned,
        Lw_Priscv_Riscv_Scpdmapoll_Error_Code_Secret_Not_Allowed
    ) with size => 2;

    for LW_PRISCV_RISCV_SCPDMAPOLL_ERROR_CODE_FIELD use
    (
        Lw_Priscv_Riscv_Scpdmapoll_Error_Code_No_Error => 0,
        Lw_Priscv_Riscv_Scpdmapoll_Error_Code_Not_Size_Aligned => 1,
        Lw_Priscv_Riscv_Scpdmapoll_Error_Code_Secret_Not_Allowed => 2
    );

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_SCPDMAPOLL_WCOUNT_FIELD is new LwU4;
    Lw_Priscv_Riscv_Scpdmapoll_Wcount_Init :
        constant LW_PRISCV_RISCV_SCPDMAPOLL_WCOUNT_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_SCPDMAPOLL_RCOUNT_FIELD is new LwU4;
    Lw_Priscv_Riscv_Scpdmapoll_Rcount_Init :
        constant LW_PRISCV_RISCV_SCPDMAPOLL_RCOUNT_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_SCPDMAPOLL_REQQ_NUM_FIELD is new LwU4;
    Lw_Priscv_Riscv_Scpdmapoll_Reqq_Num_Init :
        constant LW_PRISCV_RISCV_SCPDMAPOLL_REQQ_NUM_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_SCPDMAPOLL_REQQ_DEPTH_FIELD is new LwU4;
---------- Record Declaration ----------

    type LW_PRISCV_RISCV_SCPDMAPOLL_REGISTER is
    record
        F_Dma_Active    : LW_PRISCV_RISCV_SCPDMAPOLL_DMA_ACTIVE_FIELD;
        F_Error_Clr    : LW_PRISCV_RISCV_SCPDMAPOLL_ERROR_CLR_FIELD;
        F_Error_Code    : LW_PRISCV_RISCV_SCPDMAPOLL_ERROR_CODE_FIELD;
        F_Wcount    : LW_PRISCV_RISCV_SCPDMAPOLL_WCOUNT_FIELD;
        F_Rcount    : LW_PRISCV_RISCV_SCPDMAPOLL_RCOUNT_FIELD;
        F_Reqq_Num    : LW_PRISCV_RISCV_SCPDMAPOLL_REQQ_NUM_FIELD;
        F_Reqq_Depth    : LW_PRISCV_RISCV_SCPDMAPOLL_REQQ_DEPTH_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_SCPDMAPOLL_REGISTER use
    record
        F_Dma_Active at 0 range 0 .. 0;
        F_Error_Clr at 0 range 4 .. 4;
        F_Error_Code at 0 range 8 .. 9;
        F_Wcount at 0 range 16 .. 19;
        F_Rcount at 0 range 20 .. 23;
        F_Reqq_Num at 0 range 24 .. 27;
        F_Reqq_Depth at 0 range 28 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Scp_Secret_Mask    : constant Bar0_Addr := 16#0000_0580#;

   LW_PRISCV_RISCV_SCP_SECRET_MASK_SIZE_1 : constant := 2;
   LW_PRISCV_RISCV_SCP_SECRET_MASK_MAX_INDEX : constant := LW_PRISCV_RISCV_SCP_SECRET_MASK_SIZE_1 - 1;
   subtype LW_PRISCV_RISCV_SCP_SECRET_MASK_INDEX is LwU32 range 0 .. LW_PRISCV_RISCV_SCP_SECRET_MASK_MAX_INDEX;

   function Lw_Priscv_Riscv_Scp_Secret_Mask_Addr( Index : LW_PRISCV_RISCV_SCP_SECRET_MASK_INDEX )
                                                 return Bar0_Addr is
     (Bar0_Addr(LwU32(Lw_Priscv_Riscv_Scp_Secret_Mask) + (LwU32(Index)*4)))
     with Inline_Always,
     Global => null,
     Depends => ( Lw_Priscv_Riscv_Scp_Secret_Mask_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_SCP_SECRET_MASK_VAL_FIELD is new LwU32;
    Lw_Priscv_Riscv_Scp_Secret_Mask_Val_Init :
        constant LW_PRISCV_RISCV_SCP_SECRET_MASK_VAL_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_SCP_SECRET_MASK_REGISTER is
    record
        F_Val    : LW_PRISCV_RISCV_SCP_SECRET_MASK_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_SCP_SECRET_MASK_REGISTER use
    record
        F_Val at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

   Lw_Priscv_Riscv_Scp_Secret_Mask_Lock    : constant Bar0_Addr := 16#0000_0588#;

   LW_PRISCV_RISCV_SCP_SECRET_MASK_LOCK_SIZE_1 : constant := 2;
   LW_PRISCV_RISCV_SCP_SECRET_MASK_LOCK_MAX_INDEX : constant := LW_PRISCV_RISCV_SCP_SECRET_MASK_LOCK_SIZE_1 - 1;
   subtype LW_PRISCV_RISCV_SCP_SECRET_MASK_LOCK_INDEX is LwU32 range 0 .. LW_PRISCV_RISCV_SCP_SECRET_MASK_LOCK_MAX_INDEX;

   function Lw_Priscv_Riscv_Scp_Secret_Mask_Lock_Addr( Index : LW_PRISCV_RISCV_SCP_SECRET_MASK_LOCK_INDEX )
                                                      return Bar0_Addr is
     (Bar0_Addr(LwU32(Lw_Priscv_Riscv_Scp_Secret_Mask_Lock) + (LwU32(Index)*4)))
     with Inline_Always,
      Global => null,
     Depends => ( Lw_Priscv_Riscv_Scp_Secret_Mask_Lock_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_SCP_SECRET_MASK_LOCK_VAL_FIELD is new LwU32;
    Lw_Priscv_Riscv_Scp_Secret_Mask_Lock_Val_Init :
        constant LW_PRISCV_RISCV_SCP_SECRET_MASK_LOCK_VAL_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_SCP_SECRET_MASK_LOCK_REGISTER is
    record
        F_Val    : LW_PRISCV_RISCV_SCP_SECRET_MASK_LOCK_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_SCP_SECRET_MASK_LOCK_REGISTER use
    record
        F_Val at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Br_Retcode    : constant Bar0_Addr := 16#0000_065C#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BR_RETCODE_RESULT_FIELD is
    (
        Lw_Priscv_Riscv_Br_Retcode_Result_Init,
        Lw_Priscv_Riscv_Br_Retcode_Result_Running,
        Lw_Priscv_Riscv_Br_Retcode_Result_Fail,
        Lw_Priscv_Riscv_Br_Retcode_Result_Pass
    ) with size => 2;

    for LW_PRISCV_RISCV_BR_RETCODE_RESULT_FIELD use
    (
        Lw_Priscv_Riscv_Br_Retcode_Result_Init => 0,
        Lw_Priscv_Riscv_Br_Retcode_Result_Running => 1,
        Lw_Priscv_Riscv_Br_Retcode_Result_Fail => 2,
        Lw_Priscv_Riscv_Br_Retcode_Result_Pass => 3
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BR_RETCODE_PHASE_FIELD is
    (
        Lw_Priscv_Riscv_Br_Retcode_Phase_Entry,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Init_Device,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Load_Public_Key,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Load_Pkc_Boot_Param,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Sha_Manifest,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Verify_Manifest_Signature,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Decrypt_Manifest,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Sanitize_Manifest,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Load_Fmc,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Verify_Fmc_Digest,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Decrypt_Fmc,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Decrypt_Fusekey,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Revoke_Resource,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Configure_Fmc_Elw
    ) with size => 6;

    for LW_PRISCV_RISCV_BR_RETCODE_PHASE_FIELD use
    (
        Lw_Priscv_Riscv_Br_Retcode_Phase_Entry => 0,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Init_Device => 1,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Load_Public_Key => 2,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Load_Pkc_Boot_Param => 3,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Sha_Manifest => 4,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Verify_Manifest_Signature => 5,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Decrypt_Manifest => 6,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Sanitize_Manifest => 7,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Load_Fmc => 8,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Verify_Fmc_Digest => 9,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Decrypt_Fmc => 10,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Decrypt_Fusekey => 11,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Revoke_Resource => 12,
        Lw_Priscv_Riscv_Br_Retcode_Phase_Configure_Fmc_Elw => 13
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BR_RETCODE_SYNDROME_FIELD is
    (
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Init,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Dma_Fb_Address_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Dma_Nack_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Sha_Acquire_Mutex_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Sha_Exelwtion_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Dio_Read_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Dio_Write_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Se_Pdi_Ilwalid_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Se_Pkeyhash_Ilwalid_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Sw_Pkey_Digest_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Se_Pka_Return_Code_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Scp_Load_Secret_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Scp_Trapped_Dma_Not_Aligned_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Code_Size_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Core_Pmp_Reservation_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Data_Size_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Devicemap_Br_Unlock_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Family_Id_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Mspm_Value_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Pad_Info_Mask_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Reg_Pair_Address_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Reg_Pair_Entry_Num_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Secret_Mask_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Secret_Mask_Lock_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Signature_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Ucode_Id_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Ucode_Version_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Fmc_Digest_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Fusekey_Bad_Header_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Fusekey_Keyglob_Ilwalid_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Fusekey_Protect_Info_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Fusekey_Signature_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Kmem_Dispose_Kslot_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Kmem_Key_Slot_K3_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Kmem_Load_Kslot2Scp_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Kmem_Read_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Kmem_Write_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Iopmp_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Mmio_Error,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Ok
    ) with size => 10;

    for LW_PRISCV_RISCV_BR_RETCODE_SYNDROME_FIELD use
    (
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Init => 0,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Dma_Fb_Address_Error => 32,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Dma_Nack_Error => 33,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Sha_Acquire_Mutex_Error => 64,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Sha_Exelwtion_Error => 65,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Dio_Read_Error => 96,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Dio_Write_Error => 97,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Se_Pdi_Ilwalid_Error => 128,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Se_Pkeyhash_Ilwalid_Error => 129,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Sw_Pkey_Digest_Error => 130,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Se_Pka_Return_Code_Error => 131,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Scp_Load_Secret_Error => 160,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Scp_Trapped_Dma_Not_Aligned_Error => 161,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Code_Size_Error => 192,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Core_Pmp_Reservation_Error => 193,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Data_Size_Error => 194,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Devicemap_Br_Unlock_Error => 195,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Family_Id_Error => 196,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Mspm_Value_Error => 197,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Pad_Info_Mask_Error => 198,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Reg_Pair_Address_Error => 199,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Reg_Pair_Entry_Num_Error => 200,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Secret_Mask_Error => 201,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Secret_Mask_Lock_Error => 202,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Signature_Error => 203,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Ucode_Id_Error => 204,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Manifest_Ucode_Version_Error => 205,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Fmc_Digest_Error => 224,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Fusekey_Bad_Header_Error => 256,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Fusekey_Keyglob_Ilwalid_Error => 257,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Fusekey_Protect_Info_Error => 258,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Fusekey_Signature_Error => 259,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Kmem_Dispose_Kslot_Error => 288,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Kmem_Key_Slot_K3_Error => 289,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Kmem_Load_Kslot2Scp_Error => 290,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Kmem_Read_Error => 291,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Kmem_Write_Error => 292,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Iopmp_Error => 321,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Mmio_Error => 322,
        Lw_Priscv_Riscv_Br_Retcode_Syndrome_Ok => 1023
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BR_RETCODE_INFO_FIELD is
    (
        Lw_Priscv_Riscv_Br_Retcode_Info_Init,
        Lw_Priscv_Riscv_Br_Retcode_Info_Dma_Wait_For_Idle_Hang,
        Lw_Priscv_Riscv_Br_Retcode_Info_Sha_Hang,
        Lw_Priscv_Riscv_Br_Retcode_Info_Dio_Read_Hang,
        Lw_Priscv_Riscv_Br_Retcode_Info_Dio_Wait_Free_Entry_Hang,
        Lw_Priscv_Riscv_Br_Retcode_Info_Dio_Write_Hang,
        Lw_Priscv_Riscv_Br_Retcode_Info_Se_Acquire_Mutex_Hang,
        Lw_Priscv_Riscv_Br_Retcode_Info_Se_Pdi_Load_Hang,
        Lw_Priscv_Riscv_Br_Retcode_Info_Se_Pkeyhash_Load_Hang,
        Lw_Priscv_Riscv_Br_Retcode_Info_Pka_Poll_Result_Hang,
        Lw_Priscv_Riscv_Br_Retcode_Info_Scp_Pipeline_Reset_Hang,
        Lw_Priscv_Riscv_Br_Retcode_Info_Trapped_Dma_Hang,
        Lw_Priscv_Riscv_Br_Retcode_Info_Fusekey_Keyglob_Load_Hang,
        Lw_Priscv_Riscv_Br_Retcode_Info_Kmem_Cmd_Exelwte_Hang
    ) with size => 14;

    for LW_PRISCV_RISCV_BR_RETCODE_INFO_FIELD use
    (
        Lw_Priscv_Riscv_Br_Retcode_Info_Init => 0,
        Lw_Priscv_Riscv_Br_Retcode_Info_Dma_Wait_For_Idle_Hang => 1,
        Lw_Priscv_Riscv_Br_Retcode_Info_Sha_Hang => 2,
        Lw_Priscv_Riscv_Br_Retcode_Info_Dio_Read_Hang => 4,
        Lw_Priscv_Riscv_Br_Retcode_Info_Dio_Wait_Free_Entry_Hang => 8,
        Lw_Priscv_Riscv_Br_Retcode_Info_Dio_Write_Hang => 16,
        Lw_Priscv_Riscv_Br_Retcode_Info_Se_Acquire_Mutex_Hang => 32,
        Lw_Priscv_Riscv_Br_Retcode_Info_Se_Pdi_Load_Hang => 64,
        Lw_Priscv_Riscv_Br_Retcode_Info_Se_Pkeyhash_Load_Hang => 128,
        Lw_Priscv_Riscv_Br_Retcode_Info_Pka_Poll_Result_Hang => 256,
        Lw_Priscv_Riscv_Br_Retcode_Info_Scp_Pipeline_Reset_Hang => 512,
        Lw_Priscv_Riscv_Br_Retcode_Info_Trapped_Dma_Hang => 1024,
        Lw_Priscv_Riscv_Br_Retcode_Info_Fusekey_Keyglob_Load_Hang => 2048,
        Lw_Priscv_Riscv_Br_Retcode_Info_Kmem_Cmd_Exelwte_Hang => 4096
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BR_RETCODE_REGISTER is
    record
        F_Result    : LW_PRISCV_RISCV_BR_RETCODE_RESULT_FIELD;
        F_Phase    : LW_PRISCV_RISCV_BR_RETCODE_PHASE_FIELD;
        F_Syndrome    : LW_PRISCV_RISCV_BR_RETCODE_SYNDROME_FIELD;
        F_Info    : LW_PRISCV_RISCV_BR_RETCODE_INFO_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BR_RETCODE_REGISTER use
    record
        F_Result at 0 range 0 .. 1;
        F_Phase at 0 range 2 .. 7;
        F_Syndrome at 0 range 8 .. 17;
        F_Info at 0 range 18 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Br_Priv_Lockdown    : constant Bar0_Addr := 16#0000_0660#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BR_PRIV_LOCKDOWN_LOCK_FIELD is
    (
        Lw_Priscv_Riscv_Br_Priv_Lockdown_Lock_Unlocked,
        Lw_Priscv_Riscv_Br_Priv_Lockdown_Lock_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_BR_PRIV_LOCKDOWN_LOCK_FIELD use
    (
        Lw_Priscv_Riscv_Br_Priv_Lockdown_Lock_Unlocked => 0,
        Lw_Priscv_Riscv_Br_Priv_Lockdown_Lock_Locked => 1
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BR_PRIV_LOCKDOWN_REGISTER is
    record
        F_Lock    : LW_PRISCV_RISCV_BR_PRIV_LOCKDOWN_LOCK_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BR_PRIV_LOCKDOWN_REGISTER use
    record
        F_Lock at 0 range 0 .. 0;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Bcr_Ctrl    : constant Bar0_Addr := 16#0000_0668#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_CTRL_VALID_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Ctrl_Valid_False,
        Lw_Priscv_Riscv_Bcr_Ctrl_Valid_True
    ) with size => 1;

    for LW_PRISCV_RISCV_BCR_CTRL_VALID_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Ctrl_Valid_False => 0,
        Lw_Priscv_Riscv_Bcr_Ctrl_Valid_True => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_CTRL_CORE_SELECT_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Ctrl_Core_Select_Falcon,
        Lw_Priscv_Riscv_Bcr_Ctrl_Core_Select_Riscv
    ) with size => 1;

    for LW_PRISCV_RISCV_BCR_CTRL_CORE_SELECT_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Ctrl_Core_Select_Falcon => 0,
        Lw_Priscv_Riscv_Bcr_Ctrl_Core_Select_Riscv => 1
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_CTRL_BRFETCH_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Ctrl_Brfetch_False,
        Lw_Priscv_Riscv_Bcr_Ctrl_Brfetch_True
    ) with size => 1;

    for LW_PRISCV_RISCV_BCR_CTRL_BRFETCH_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Ctrl_Brfetch_False => 0,
        Lw_Priscv_Riscv_Bcr_Ctrl_Brfetch_True => 1
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BCR_CTRL_REGISTER is
    record
        F_Valid    : LW_PRISCV_RISCV_BCR_CTRL_VALID_FIELD;
        F_Core_Select    : LW_PRISCV_RISCV_BCR_CTRL_CORE_SELECT_FIELD;
        F_Brfetch    : LW_PRISCV_RISCV_BCR_CTRL_BRFETCH_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BCR_CTRL_REGISTER use
    record
        F_Valid at 0 range 0 .. 0;
        F_Core_Select at 0 range 4 .. 4;
        F_Brfetch at 0 range 8 .. 8;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Bcr_Dmacfg    : constant Bar0_Addr := 16#0000_066C#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMACFG_TARGET_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Dmacfg_Target_Local_Fb,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Target_Coherent_Sysmem,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Target_Noncoherent_Sysmem
    ) with size => 2;

    for LW_PRISCV_RISCV_BCR_DMACFG_TARGET_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Dmacfg_Target_Local_Fb => 0,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Target_Coherent_Sysmem => 1,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Target_Noncoherent_Sysmem => 2
    );

---------- Enum Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMACFG_LOCK_FIELD is
    (
        Lw_Priscv_Riscv_Bcr_Dmacfg_Lock_Unlocked,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Lock_Locked
    ) with size => 1;

    for LW_PRISCV_RISCV_BCR_DMACFG_LOCK_FIELD use
    (
        Lw_Priscv_Riscv_Bcr_Dmacfg_Lock_Unlocked => 0,
        Lw_Priscv_Riscv_Bcr_Dmacfg_Lock_Locked => 1
    );

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMACFG_REGISTER is
    record
        F_Target    : LW_PRISCV_RISCV_BCR_DMACFG_TARGET_FIELD;
        F_Lock    : LW_PRISCV_RISCV_BCR_DMACFG_LOCK_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BCR_DMACFG_REGISTER use
    record
        F_Target at 0 range 0 .. 1;
        F_Lock at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Bcr_Dmacfg_Sec    : constant Bar0_Addr := 16#0000_0694#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMACFG_SEC_WPRID_FIELD is new LwU2;
    Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Wprid_Init :
        constant LW_PRISCV_RISCV_BCR_DMACFG_SEC_WPRID_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMACFG_SEC_REGISTER is
    record
        F_Wprid    : LW_PRISCV_RISCV_BCR_DMACFG_SEC_WPRID_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BCR_DMACFG_SEC_REGISTER use
    record
        F_Wprid at 0 range 0 .. 1;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Bcr_Dmaaddr_Pkcparam_Lo    : constant Bar0_Addr := 16#0000_0670#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMAADDR_PKCPARAM_LO_VAL_FIELD is new LwU32;
    Lw_Priscv_Riscv_Bcr_Dmaaddr_Pkcparam_Lo_Val_Init :
        constant LW_PRISCV_RISCV_BCR_DMAADDR_PKCPARAM_LO_VAL_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMAADDR_PKCPARAM_LO_REGISTER is
    record
        F_Val    : LW_PRISCV_RISCV_BCR_DMAADDR_PKCPARAM_LO_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BCR_DMAADDR_PKCPARAM_LO_REGISTER use
    record
        F_Val at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Bcr_Dmaaddr_Pkcparam_Hi    : constant Bar0_Addr := 16#0000_0674#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMAADDR_PKCPARAM_HI_VAL_FIELD is new LwU7;
    Lw_Priscv_Riscv_Bcr_Dmaaddr_Pkcparam_Hi_Val_Init :
        constant LW_PRISCV_RISCV_BCR_DMAADDR_PKCPARAM_HI_VAL_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMAADDR_PKCPARAM_HI_REGISTER is
    record
        F_Val    : LW_PRISCV_RISCV_BCR_DMAADDR_PKCPARAM_HI_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BCR_DMAADDR_PKCPARAM_HI_REGISTER use
    record
        F_Val at 0 range 0 .. 6;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Bcr_Dmaaddr_Fmccode_Lo    : constant Bar0_Addr := 16#0000_0678#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_LO_VAL_FIELD is new LwU32;
    Lw_Priscv_Riscv_Bcr_Dmaaddr_Fmccode_Lo_Val_Init :
        constant LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_LO_VAL_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_LO_REGISTER is
    record
        F_Val    : LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_LO_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_LO_REGISTER use
    record
        F_Val at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Bcr_Dmaaddr_Fmccode_Hi    : constant Bar0_Addr := 16#0000_067C#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_HI_VAL_FIELD is new LwU7;
    Lw_Priscv_Riscv_Bcr_Dmaaddr_Fmccode_Hi_Val_Init :
        constant LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_HI_VAL_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_HI_REGISTER is
    record
        F_Val    : LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_HI_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_HI_REGISTER use
    record
        F_Val at 0 range 0 .. 6;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Bcr_Dmaaddr_Fmcdata_Lo    : constant Bar0_Addr := 16#0000_0680#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_LO_VAL_FIELD is new LwU32;
    Lw_Priscv_Riscv_Bcr_Dmaaddr_Fmcdata_Lo_Val_Init :
        constant LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_LO_VAL_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_LO_REGISTER is
    record
        F_Val    : LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_LO_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_LO_REGISTER use
    record
        F_Val at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Bcr_Dmaaddr_Fmcdata_Hi    : constant Bar0_Addr := 16#0000_0684#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_HI_VAL_FIELD is new LwU7;
    Lw_Priscv_Riscv_Bcr_Dmaaddr_Fmcdata_Hi_Val_Init :
        constant LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_HI_VAL_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_HI_REGISTER is
    record
        F_Val    : LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_HI_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_HI_REGISTER use
    record
        F_Val at 0 range 0 .. 6;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Bcr_Dmaaddr_Pubkey_Lo    : constant Bar0_Addr := 16#0000_0688#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMAADDR_PUBKEY_LO_VAL_FIELD is new LwU32;
    Lw_Priscv_Riscv_Bcr_Dmaaddr_Pubkey_Lo_Val_Init :
        constant LW_PRISCV_RISCV_BCR_DMAADDR_PUBKEY_LO_VAL_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMAADDR_PUBKEY_LO_REGISTER is
    record
        F_Val    : LW_PRISCV_RISCV_BCR_DMAADDR_PUBKEY_LO_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BCR_DMAADDR_PUBKEY_LO_REGISTER use
    record
        F_Val at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Priscv_Riscv_Bcr_Dmaaddr_Pubkey_Hi    : constant Bar0_Addr := 16#0000_068C#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMAADDR_PUBKEY_HI_VAL_FIELD is new LwU7;
    Lw_Priscv_Riscv_Bcr_Dmaaddr_Pubkey_Hi_Val_Init :
        constant LW_PRISCV_RISCV_BCR_DMAADDR_PUBKEY_HI_VAL_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_BCR_DMAADDR_PUBKEY_HI_REGISTER is
    record
        F_Val    : LW_PRISCV_RISCV_BCR_DMAADDR_PUBKEY_HI_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_BCR_DMAADDR_PUBKEY_HI_REGISTER use
    record
        F_Val at 0 range 0 .. 6;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

   Lw_Priscv_Riscv_Devicemap_Riscvmmode    : constant Bar0_Addr := 16#0000_0610#;

------------------------------------------------------------------------------------------------------------
-- MANUAL EDIT START --
------------------------------------------------------------------------------------------------------------
   LW_PRISCV_DEVICE_MAP_GROUP_MMODE          : constant := 16#0#;
   LW_PRISCV_DEVICE_MAP_GROUP_RISCV_CTL      : constant := 16#1#;
   LW_PRISCV_DEVICE_MAP_GROUP_PIC            : constant := 16#2#;
   LW_PRISCV_DEVICE_MAP_GROUP_TIMER          : constant := 16#3#;
   LW_PRISCV_DEVICE_MAP_GROUP_HOSTIF         : constant := 16#4#;
   LW_PRISCV_DEVICE_MAP_GROUP_DMA            : constant := 16#5#;
   LW_PRISCV_DEVICE_MAP_GROUP_PMB            : constant := 16#6#;
   LW_PRISCV_DEVICE_MAP_GROUP_DIO            : constant := 16#7#;
   LW_PRISCV_DEVICE_MAP_GROUP_KEY            : constant := 16#8#;
   LW_PRISCV_DEVICE_MAP_GROUP_DEBUG          : constant := 16#9#;
   LW_PRISCV_DEVICE_MAP_GROUP_SHA            : constant := 16#a#;
   LW_PRISCV_DEVICE_MAP_GROUP_KMEM           : constant := 16#b#;
   LW_PRISCV_DEVICE_MAP_GROUP_BROM           : constant := 16#c#;
   LW_PRISCV_DEVICE_MAP_GROUP_ROM_PATCH      : constant := 16#d#;
   LW_PRISCV_DEVICE_MAP_GROUP_IOPMP          : constant := 16#e#;
   LW_PRISCV_DEVICE_MAP_GROUP_NOACCESS       : constant := 16#f#;
   LW_PRISCV_DEVICE_MAP_GROUP_SCP            : constant := 16#10#;
   LW_PRISCV_DEVICE_MAP_GROUP_FBIF           : constant := 16#11#;
   LW_PRISCV_DEVICE_MAP_GROUP_FALCON_ONLY    : constant := 16#12#;
   LW_PRISCV_DEVICE_MAP_GROUP_PRGN_CTL       : constant := 16#13#;
   LW_PRISCV_DEVICE_MAP_GROUP_SCRATCH_GROUP0 : constant := 16#14#;
   LW_PRISCV_DEVICE_MAP_GROUP_SCRATCH_GROUP1 : constant := 16#15#;
   LW_PRISCV_DEVICE_MAP_GROUP_SCRATCH_GROUP2 : constant := 16#16#;
   LW_PRISCV_DEVICE_MAP_GROUP_SCRATCH_GROUP3 : constant := 16#17#;
   LW_PRISCV_DEVICE_MAP_GROUP_PLM            : constant := 16#18#;
   LW_PRISCV_DEVICE_MAP_GROUP_HUB_DIO        : constant := 16#19#;


--     LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_VAL_INIT      : constant LwU32 := 16#3333_3333#;
   LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_READ_ENABLE   : constant       := 16#1#;
   LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_READ_DISABLE  : constant       := 16#0#;
   LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_WRITE_ENABLE  : constant       := 16#1#;
   LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_WRITE_DISABLE : constant       := 16#0#;
   LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_LOCK_LOCKED   : constant       := 16#1#;
   LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_LOCK_UNLOCKED : constant       := 16#0#;
   ------------------------------------------------------------------------------------------------------------

   LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_SIZE_1 : constant := 8;
   LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_MAX_INDEX : constant := LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_SIZE_1 - 1;
   subtype LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_INDEX is LwU32 range 0 .. LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_MAX_INDEX;

   function Lw_Priscv_Riscv_Devicemap_Riscvmmode_Addr( Index : LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_INDEX )
                                                      return Bar0_Addr is
     (Bar0_Addr(LwU32(Lw_Priscv_Riscv_Devicemap_Riscvmmode) + (LwU32(Index)*8)))
     with Inline_Always,
     Global => null,
     Depends => ( Lw_Priscv_Riscv_Devicemap_Riscvmmode_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

--    type LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_VAL_FIELD is new LwU32;
--    Lw_Priscv_Riscv_Devicemap_Riscvmmode_Val_Init :
--        constant LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_VAL_FIELD
--        := 16#3333_3333#;

   type LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_READ_FIELD is new LwU1;

   type LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_WRITE_FIELD is new LwU1;

   type LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_LOCK_FIELD is new LwU1;

   type LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_VAL_FIELD is
    record
         Read : LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_READ_FIELD;
         Write : LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_WRITE_FIELD;
         Unused : LwU1;
         Lock : LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_LOCK_FIELD;
    end record
   with  Size => 4;

   for LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_VAL_FIELD use
    record
         Read at 0 range 0 .. 0;
         Write at 0 range 1 .. 1;
         Unused at 0 range 2 .. 2;
         Lock at 0 range 3 .. 3;
    end record;
   ------------------------------------------------------------------------------------------------------------
   -- MANUAL EDIT END --
   ------------------------------------------------------------------------------------------------------------

   ---------- Record Declaration ----------

   type LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_VAL_FIELD_ARRAY is array(LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_INDEX) of LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_VAL_FIELD;
   for LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_VAL_FIELD_ARRAY'Component_Size use 4;

   type LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_REGISTER is
    record
         F_Val    : LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_VAL_FIELD_ARRAY;
    end record
   with  Size => 32, Object_size => 32, Alignment => 4;

   for LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_REGISTER use
      record
         F_Val at 0 range 0 .. 31;
      end record;

--      type LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_REGISTER is
--      record
--          F_Val    : LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_VAL_FIELD;
--      end record
--          with  Size => 32, Object_size => 32, Alignment => 4;
--
--        for LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_REGISTER use
--        record
--            F_Val at 0 range 0 .. 31;
--        end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

   Lw_Priscv_Riscv_Devicemap_Riscvsubmmode    : constant Bar0_Addr := 16#0000_0614#;

   LW_PRISCV_RISCV_DEVICEMAP_RISCVSUBMMODE_SIZE_1 : constant := 8;
   LW_PRISCV_RISCV_DEVICEMAP_RISCVSUBMMODE_MAX_INDEX : constant := LW_PRISCV_RISCV_DEVICEMAP_RISCVSUBMMODE_SIZE_1 - 1;
   subtype LW_PRISCV_RISCV_DEVICEMAP_RISCVSUBMMODE_INDEX is LwU32 range 0 .. LW_PRISCV_RISCV_DEVICEMAP_RISCVSUBMMODE_MAX_INDEX;

   function Lw_Priscv_Riscv_Devicemap_Riscvsubmmode_Addr( Index : LW_PRISCV_RISCV_DEVICEMAP_RISCVSUBMMODE_INDEX )
                                                         return Bar0_Addr is
     (Bar0_Addr(LwU32(Lw_Priscv_Riscv_Devicemap_Riscvsubmmode) + (LwU32(Index)*8)))
     with Inline_Always,
     Global => null,
     Depends => ( Lw_Priscv_Riscv_Devicemap_Riscvsubmmode_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PRISCV_RISCV_DEVICEMAP_RISCVSUBMMODE_VAL_FIELD is new LwU32;
    Lw_Priscv_Riscv_Devicemap_Riscvsubmmode_Val_Init :
        constant LW_PRISCV_RISCV_DEVICEMAP_RISCVSUBMMODE_VAL_FIELD
        := 16#3333_3333#;

---------- Record Declaration ----------

    type LW_PRISCV_RISCV_DEVICEMAP_RISCVSUBMMODE_REGISTER is
    record
        F_Val    : LW_PRISCV_RISCV_DEVICEMAP_RISCVSUBMMODE_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PRISCV_RISCV_DEVICEMAP_RISCVSUBMMODE_REGISTER use
    record
        F_Val at 0 range 0 .. 31;
    end record;

   ------------------------------------------------------------------------------------------------------

end Dev_Riscv_Pri;
