-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Lw_Types.Reg_Addr_Types; use Lw_Types.Reg_Addr_Types;

--******************************  DEV_GRAPHICS_NOBUNDLE  ******************************--

package Dev_Graphics_Nobundle_Ada
with SPARK_MODE => On
is


---------- Register Range Subtype Declaration ----------
    subtype LW_PGRAPH_BAR0_ADDR is BAR0_ADDR range 16#0040_0000# .. 16#005F_FFFF#;
------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0040_928C#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 2;
        F_Read_Violation at 0 range 3 .. 3;
        F_Write_Protection at 0 range 4 .. 6;
        F_Write_Violation at 0 range 7 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0040_9290#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 2;
        F_Read_Violation at 0 range 3 .. 3;
        F_Write_Protection at 0 range 4 .. 6;
        F_Write_Violation at 0 range 7 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0040_9294#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 2;
        F_Read_Violation at 0 range 3 .. 3;
        F_Write_Protection at 0 range 4 .. 6;
        F_Write_Violation at 0 range 7 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0040_9950#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_Level2_Enabled_Fuse1,
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_All_Levels_Enabled_Fuse0
    ) with size => 4;

    for LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_Level2_Enabled_Fuse1 => 12,
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_All_Levels_Enabled_Fuse0 => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Enable_None,
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Enable_All
    ) with size => 20;

    for LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Enable_None => 0,
        Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Enable_All => 1048575
    );

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0040_9760#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Write_Protection_Level1_Enabled_Fuse1,
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Write_Protection_All_Levels_Enabled_Fuse0
    ) with size => 4;

    for LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Write_Protection_Level1_Enabled_Fuse1 => 12,
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Write_Protection_All_Levels_Enabled_Fuse0 => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#0008_0144#;

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0040_9A28#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Write_Protection_Default_Priv_Level,
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Write_Protection_Default_Priv_Level => 12,
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0040_9A30#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_Default_Priv_Level,
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 12,
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_Default_Priv_Level,
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_Default_Priv_Level => 12,
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0041_A28C#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 2;
        F_Read_Violation at 0 range 3 .. 3;
        F_Write_Protection at 0 range 4 .. 6;
        F_Write_Violation at 0 range 7 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0041_A290#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 2;
        F_Read_Violation at 0 range 3 .. 3;
        F_Write_Protection at 0 range 4 .. 6;
        F_Write_Violation at 0 range 7 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0041_A294#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 2;
        F_Read_Violation at 0 range 3 .. 3;
        F_Write_Protection at 0 range 4 .. 6;
        F_Write_Violation at 0 range 7 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0041_A950#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_Level2_Enabled_Fuse1,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_All_Levels_Enabled_Fuse0
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_Level2_Enabled_Fuse1 => 12,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_All_Levels_Enabled_Fuse0 => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Enable_None,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Enable_All
    ) with size => 20;

    for LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Enable_None => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Enable_All => 1048575
    );

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0041_A760#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Write_Protection_Level1_Enabled_Fuse1,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Write_Protection_All_Levels_Enabled_Fuse0
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Write_Protection_Level1_Enabled_Fuse1 => 12,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Write_Protection_All_Levels_Enabled_Fuse0 => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#0008_0144#;

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0041_AA28#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Write_Protection_Default_Priv_Level,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Write_Protection_Default_Priv_Level => 12,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0041_AA30#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_Default_Priv_Level,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 12,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_Default_Priv_Level,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_Default_Priv_Level => 12,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
         F_Source_Read_Control    :
         LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
         F_Source_Write_Control    :
         LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0041_8300#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Write_Protection_Fuse1,
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Write_Protection_Default_Priv_Level,
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Write_Protection_Fuse0
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Write_Protection_Fuse1 => 8,
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Write_Protection_Default_Priv_Level => 12,
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Write_Protection_Fuse0 => 15
    );

    Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled :
        constant LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD
        := Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Write_Protection_Fuse1;

    Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Write_Protection_All_Levels_Enabled :
        constant LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD
        := Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Write_Protection_Fuse0;

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0041_9DD0#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_Level2_Enabled_Fuse1,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_All_Levels_Enabled_Fuse0
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_Level2_Enabled_Fuse1 => 12,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_All_Levels_Enabled_Fuse0 => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Enable_None,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Enable_All
    ) with size => 20;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Enable_None => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Source_Enable_All => 1048575
    );

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
         F_Source_Write_Control    :
         LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0041_9D68#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Write_Protection_Default_Priv_Level,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Write_Protection_Default_Priv_Level => 12,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_REGISTER use
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

   Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask    :
   constant LW_PGRAPH_BAR0_ADDR := 16#0041_9D70#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_Default_Priv_Level,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 12,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_Default_Priv_Level,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_Default_Priv_Level => 12,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
         F_Write_Protection    :
         LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
         F_Source_Read_Control    :
         LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
         F_Source_Write_Control    :
         LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask    : constant LW_PGRAPH_BAR0_ADDR := 16#0041_9B94#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Write_Protection_Pl2_And_Pl3_Enabled,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Write_Protection_Pl2_And_Pl3_Enabled => 12,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Source_Enable_Ctxsw_Ucode_Enabled,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Source_Enable_All_Sources_Enabled
    ) with size => 20;

    for LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Source_Enable_Ctxsw_Ucode_Enabled => 118,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Source_Enable_All_Sources_Enabled => 1048575
    );

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
         F_Source_Write_Control    :
         LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_REGISTER use
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


    Lw_Pgraph_Pri_Fecs_Falcon_Addr    : constant LW_PGRAPH_BAR0_ADDR := 16#0040_90AC#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_ADDR_LSB_FIELD is new LwU6;
    Lw_Pgraph_Pri_Fecs_Falcon_Addr_Lsb_Init :
        constant LW_PGRAPH_PRI_FECS_FALCON_ADDR_LSB_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_ADDR_MSB_FIELD is new LwU6;
    Lw_Pgraph_Pri_Fecs_Falcon_Addr_Msb_Init :
        constant LW_PGRAPH_PRI_FECS_FALCON_ADDR_MSB_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_FECS_FALCON_ADDR_REGISTER is
    record
        F_Lsb    : LW_PGRAPH_PRI_FECS_FALCON_ADDR_LSB_FIELD;
        F_Msb    : LW_PGRAPH_PRI_FECS_FALCON_ADDR_MSB_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_FECS_FALCON_ADDR_REGISTER use
    record
        F_Lsb at 0 range 0 .. 5;
        F_Msb at 0 range 6 .. 11;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Addr    : constant LW_PGRAPH_BAR0_ADDR := 16#0041_A0AC#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_ADDR_LSB_FIELD is new LwU6;
    Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Addr_Lsb_Init :
        constant LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_ADDR_LSB_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_ADDR_MSB_FIELD is new LwU6;
    Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Addr_Msb_Init :
        constant LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_ADDR_MSB_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_ADDR_REGISTER is
    record
        F_Lsb    : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_ADDR_LSB_FIELD;
        F_Msb    : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_ADDR_MSB_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_ADDR_REGISTER use
    record
        F_Lsb at 0 range 0 .. 5;
        F_Msb at 0 range 6 .. 11;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Select    : constant LW_PGRAPH_BAR0_ADDR := 16#0041_9D20#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_SELECT_INDEX_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Select_Index_Lane0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Select_Index_Lane1,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Select_Index_All
    ) with size => 6;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_SELECT_INDEX_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Select_Index_Lane0 => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Select_Index_Lane1 => 1,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Select_Index_All => 63
    );

---------- Enum Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_SELECT_AUTO_INCREMENT_FIELD is
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Select_Auto_Increment_Disabled,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Select_Auto_Increment_Enabled
    ) with size => 1;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_SELECT_AUTO_INCREMENT_FIELD use
    (
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Select_Auto_Increment_Disabled => 0,
        Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Select_Auto_Increment_Enabled => 1
    );

---------- Record Declaration ----------

    type LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_SELECT_REGISTER is
    record
        F_Index    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_SELECT_INDEX_FIELD;
        F_Auto_Increment    : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_SELECT_AUTO_INCREMENT_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_SELECT_REGISTER use
    record
        F_Index at 0 range 0 .. 5;
        F_Auto_Increment at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
end Dev_Graphics_Nobundle_Ada;
