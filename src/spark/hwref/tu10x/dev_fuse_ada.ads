-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2019 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Lw_Types.Reg_Addr_Types; use Lw_Types.Reg_Addr_Types;

--******************************  DEV_FUSE  ******************************--

package Dev_Fuse_Ada
with SPARK_MODE => On
is


---------- Register Range Subtype Declaration ----------
    subtype LW_FUSE_BAR0_ADDR is BAR0_ADDR range 16#0002_1000# .. 16#0002_1FFF#;
------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------
    Lw_Fuse_Floorsweep_Priv_Level_Mask    : constant LW_FUSE_BAR0_ADDR := 16#0002_10DC#;
------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------
    type LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 4;
    for LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 15
    );
---------- Enum Declaration ----------
    type LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;
    for LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );
---------- Enum Declaration ----------
    type LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;
    for LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );
---------- Enum Declaration ----------
    type LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;
    for LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );
---------- Enum Declaration ----------
    type LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;
    for LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );
---------- Enum Declaration ----------
    type LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;
    for LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Fuse_Floorsweep_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );
---------- Record Field Type Declaration ----------
    type LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Fuse_Floorsweep_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;
---------- Record Declaration ----------
    type LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;
    for LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Fuse_Iddqinfo_Priv_Level_Mask    : constant LW_FUSE_BAR0_ADDR := 16#0002_10F0#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Write_Protection_Level3_Disable,
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Write_Protection_Level3_Enable,
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Write_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Write_Protection_Level3_Disable => 0,
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Write_Protection_Level3_Enable => 1,
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Write_Protection_Default_Priv_Level => 15
    );

    Lw_Fuse_Iddqinfo_Priv_Level_Mask_Write_Protection_All_Levels_Enabled :
        constant LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD
        := Lw_Fuse_Iddqinfo_Priv_Level_Mask_Write_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Fuse_Iddqinfo_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Fuse_Iddqinfo_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Fuse_Kappainfo_Priv_Level_Mask    : constant LW_FUSE_BAR0_ADDR := 16#0002_10D4#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Write_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Write_Protection_Default_Priv_Level => 15
    );

    Lw_Fuse_Kappainfo_Priv_Level_Mask_Write_Protection_All_Levels_Enabled :
        constant LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD
        := Lw_Fuse_Kappainfo_Priv_Level_Mask_Write_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Fuse_Kappainfo_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Fuse_Kappainfo_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Fuse_Speedoinfo_Priv_Level_Mask    : constant LW_FUSE_BAR0_ADDR := 16#0002_10EC#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Write_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Write_Protection_Default_Priv_Level => 15
    );

    Lw_Fuse_Speedoinfo_Priv_Level_Mask_Write_Protection_All_Levels_Enabled :
        constant LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD
        := Lw_Fuse_Speedoinfo_Priv_Level_Mask_Write_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Fuse_Speedoinfo_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Fuse_Speedoinfo_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Fuse_Sram_Vmin_Priv_Level_Mask    : constant LW_FUSE_BAR0_ADDR := 16#0002_10E8#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Write_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Write_Protection_Default_Priv_Level => 15
    );

    Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Write_Protection_All_Levels_Enabled :
        constant LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD
        := Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Write_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_REGISTER use
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

    LW_FUSE_OPT_FUSE_UCODE_ACR_HS_REV_ADDR    : constant LW_FUSE_BAR0_ADDR := 16#0002_1254#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_FUSE_OPT_FUSE_UCODE_ACR_HS_REV_DATA_FIELD is new LwU3;
    Lw_Fuse_Opt_Fuse_Ucode_Acr_Hs_Rev_Data_Init :
        constant LW_FUSE_OPT_FUSE_UCODE_ACR_HS_REV_DATA_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_FUSE_OPT_FUSE_UCODE_ACR_HS_REV_REGISTER is
    record
        F_Data    : LW_FUSE_OPT_FUSE_UCODE_ACR_HS_REV_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_FUSE_OPT_FUSE_UCODE_ACR_HS_REV_REGISTER use
    record
        F_Data at 0 range 0 .. 2;
    end record;

------------------------------------------------------------------------------------------------------
end Dev_Fuse_Ada;
