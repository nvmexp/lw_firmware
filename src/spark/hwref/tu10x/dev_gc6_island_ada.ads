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

--******************************  DEV_GC6_ISLAND  ******************************--

package Dev_Gc6_Island_Ada
with SPARK_MODE => On
is


---------- Register Range Subtype Declaration ----------
    subtype LW_PGC6_BAR0_ADDR is BAR0_ADDR range 16#0011_8000# .. 16#0011_8FFF#;
------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask    : constant LW_PGC6_BAR0_ADDR := 16#0011_8150#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is
    (
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Source_Enable_None,
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Source_Enable_All_Sources_Enabled
    ) with size => 20;

    for LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD use
    (
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Source_Enable_None => 0,
        Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Source_Enable_All_Sources_Enabled => 1048575
    );

---------- Record Declaration ----------

    type LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_REGISTER use
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
end Dev_Gc6_Island_Ada;
