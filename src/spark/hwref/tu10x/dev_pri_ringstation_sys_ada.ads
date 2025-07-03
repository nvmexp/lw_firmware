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

--******************************  DEV_PRI_RINGSTATION_SYS  ******************************--

package Dev_Pri_Ringstation_Sys_Ada
with SPARK_MODE => On
is

---------- Register Range Subtype Declaration ----------
    subtype LW_PPRIV_SYS_BAR0_ADDR is BAR0_ADDR range 16#0012_2000# .. 16#0012_27FF#;
------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_222C#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Read_Protection_Level3_Enabled,
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Read_Protection_Level3_Enabled => 8,
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Write_Protection_Level3_Enable,
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Write_Protection_Level2_Enable,
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Write_Protection_Level3_Enable => 8,
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Write_Protection_Level2_Enable => 12,
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Source_Read_Control_Lower,
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Source_Read_Control_Block
    ) with size => 1;

    for LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Source_Read_Control_Lower => 0,
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Source_Read_Control_Block => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Source_Write_Control_Lower,
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Source_Write_Control_Block
    ) with size => 1;

    for LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Source_Write_Control_Lower => 0,
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Source_Write_Control_Block => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is
    (
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Source_Enable_None,
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Source_Enable_Fecs_Iff_Sec2
    ) with size => 20;

    for LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD use
    (
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Source_Enable_None => 0,
        Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Source_Enable_Fecs_Iff_Sec2 => 84
    );

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Ppriv_Sys_Pri_Decode_Trap12_Match    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_2430#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_ADDR_FIELD is new LwU26;
    Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Addr_I :
        constant LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_ADDR_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_SUBID_FIELD is new LwU6;
    Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Subid_I :
        constant LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_SUBID_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_REGISTER is
    record
        F_Addr    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_ADDR_FIELD;
        F_Subid    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_SUBID_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_REGISTER use
    record
        F_Addr at 0 range 0 .. 25;
        F_Subid at 0 range 26 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap13_Match    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_2434#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_ADDR_FIELD is new LwU26;
    Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Addr_I :
        constant LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_ADDR_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_SUBID_FIELD is new LwU6;
    Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Subid_I :
        constant LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_SUBID_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_REGISTER is
    record
        F_Addr    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_ADDR_FIELD;
        F_Subid    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_SUBID_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_REGISTER use
    record
        F_Addr at 0 range 0 .. 25;
        F_Subid at 0 range 26 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap14_Match    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_2438#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_ADDR_FIELD is new LwU26;
    Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Addr_I :
        constant LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_ADDR_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_SUBID_FIELD is new LwU6;
    Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Subid_I :
        constant LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_SUBID_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_REGISTER is
    record
        F_Addr    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_ADDR_FIELD;
        F_Subid    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_SUBID_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_REGISTER use
    record
        F_Addr at 0 range 0 .. 25;
        F_Subid at 0 range 26 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_26B0#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_IGNORE_READ_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Read_Matched,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Read_Ignored
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_IGNORE_READ_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Read_Matched => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Read_Ignored => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_IGNORE_WRITE_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Write_Matched,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Write_Ignored
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_IGNORE_WRITE_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Write_Matched => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Write_Ignored => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_IGNORE_WRITE_ACK_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Write_Ack_Matched,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Write_Ack_Ignored
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_IGNORE_WRITE_ACK_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Write_Ack_Matched => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Write_Ack_Ignored => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_IGNORE_WORD_ACCESS_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Word_Access_Matched,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Word_Access_Ignored
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_IGNORE_WORD_ACCESS_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Word_Access_Matched => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Word_Access_Ignored => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_IGNORE_BYTE_ACCESS_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Byte_Access_Matched,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Byte_Access_Ignored
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_IGNORE_BYTE_ACCESS_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Byte_Access_Matched => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Byte_Access_Ignored => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_ILWERT_ADDR_MATCH_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ilwert_Addr_Match_Normal,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ilwert_Addr_Match_Ilwerted
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_ILWERT_ADDR_MATCH_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ilwert_Addr_Match_Normal => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ilwert_Addr_Match_Ilwerted => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_ILWERT_SUBID_MATCH_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ilwert_Subid_Match_Normal,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ilwert_Subid_Match_Ilwerted
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_ILWERT_SUBID_MATCH_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ilwert_Subid_Match_Normal => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ilwert_Subid_Match_Ilwerted => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_TRAP_APPLICATION_LEVEL0_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Trap_Application_Level0_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Trap_Application_Level0_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_TRAP_APPLICATION_LEVEL0_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Trap_Application_Level0_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Trap_Application_Level0_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_TRAP_APPLICATION_LEVEL1_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Trap_Application_Level1_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Trap_Application_Level1_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_TRAP_APPLICATION_LEVEL1_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Trap_Application_Level1_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Trap_Application_Level1_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_TRAP_APPLICATION_LEVEL2_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Trap_Application_Level2_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Trap_Application_Level2_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_TRAP_APPLICATION_LEVEL2_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Trap_Application_Level2_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Trap_Application_Level2_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_SUBID_CTL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Subid_Ctl_Masked,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Subid_Ctl_Compared
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_SUBID_CTL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Subid_Ctl_Masked => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Subid_Ctl_Compared => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_COMPARE_CTL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Compare_Ctl_Masked,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Compare_Ctl_Compared
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_COMPARE_CTL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Compare_Ctl_Masked => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Compare_Ctl_Compared => 1
    );

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_REGISTER is
    record
        F_Ignore_Read    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_IGNORE_READ_FIELD;
        F_Ignore_Write    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_IGNORE_WRITE_FIELD;
        F_Ignore_Write_Ack    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_IGNORE_WRITE_ACK_FIELD;
        F_Ignore_Word_Access    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_IGNORE_WORD_ACCESS_FIELD;
        F_Ignore_Byte_Access    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_IGNORE_BYTE_ACCESS_FIELD;
        F_Ilwert_Addr_Match    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_ILWERT_ADDR_MATCH_FIELD;
        F_Ilwert_Subid_Match    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_ILWERT_SUBID_MATCH_FIELD;
        F_Trap_Application_Level0    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_TRAP_APPLICATION_LEVEL0_FIELD;
        F_Trap_Application_Level1    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_TRAP_APPLICATION_LEVEL1_FIELD;
        F_Trap_Application_Level2    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_TRAP_APPLICATION_LEVEL2_FIELD;
        F_Subid_Ctl    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_SUBID_CTL_FIELD;
        F_Compare_Ctl    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_COMPARE_CTL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_REGISTER use
    record
        F_Ignore_Read at 0 range 4 .. 4;
        F_Ignore_Write at 0 range 5 .. 5;
        F_Ignore_Write_Ack at 0 range 6 .. 6;
        F_Ignore_Word_Access at 0 range 8 .. 8;
        F_Ignore_Byte_Access at 0 range 9 .. 9;
        F_Ilwert_Addr_Match at 0 range 12 .. 12;
        F_Ilwert_Subid_Match at 0 range 13 .. 13;
        F_Trap_Application_Level0 at 0 range 14 .. 14;
        F_Trap_Application_Level1 at 0 range 15 .. 15;
        F_Trap_Application_Level2 at 0 range 16 .. 16;
        F_Subid_Ctl at 0 range 30 .. 30;
        F_Compare_Ctl at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_26B4#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_IGNORE_READ_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Read_Matched,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Read_Ignored
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_IGNORE_READ_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Read_Matched => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Read_Ignored => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_IGNORE_WRITE_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Write_Matched,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Write_Ignored
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_IGNORE_WRITE_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Write_Matched => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Write_Ignored => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_IGNORE_WRITE_ACK_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Write_Ack_Matched,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Write_Ack_Ignored
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_IGNORE_WRITE_ACK_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Write_Ack_Matched => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Write_Ack_Ignored => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_IGNORE_WORD_ACCESS_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Word_Access_Matched,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Word_Access_Ignored
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_IGNORE_WORD_ACCESS_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Word_Access_Matched => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Word_Access_Ignored => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_IGNORE_BYTE_ACCESS_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Byte_Access_Matched,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Byte_Access_Ignored
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_IGNORE_BYTE_ACCESS_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Byte_Access_Matched => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Byte_Access_Ignored => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_ILWERT_ADDR_MATCH_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ilwert_Addr_Match_Normal,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ilwert_Addr_Match_Ilwerted
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_ILWERT_ADDR_MATCH_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ilwert_Addr_Match_Normal => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ilwert_Addr_Match_Ilwerted => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_ILWERT_SUBID_MATCH_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ilwert_Subid_Match_Normal,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ilwert_Subid_Match_Ilwerted
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_ILWERT_SUBID_MATCH_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ilwert_Subid_Match_Normal => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ilwert_Subid_Match_Ilwerted => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_TRAP_APPLICATION_LEVEL0_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Trap_Application_Level0_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Trap_Application_Level0_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_TRAP_APPLICATION_LEVEL0_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Trap_Application_Level0_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Trap_Application_Level0_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_TRAP_APPLICATION_LEVEL1_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Trap_Application_Level1_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Trap_Application_Level1_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_TRAP_APPLICATION_LEVEL1_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Trap_Application_Level1_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Trap_Application_Level1_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_TRAP_APPLICATION_LEVEL2_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Trap_Application_Level2_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Trap_Application_Level2_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_TRAP_APPLICATION_LEVEL2_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Trap_Application_Level2_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Trap_Application_Level2_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_SUBID_CTL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Subid_Ctl_Masked,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Subid_Ctl_Compared
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_SUBID_CTL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Subid_Ctl_Masked => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Subid_Ctl_Compared => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_COMPARE_CTL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Compare_Ctl_Masked,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Compare_Ctl_Compared
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_COMPARE_CTL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Compare_Ctl_Masked => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Compare_Ctl_Compared => 1
    );

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_REGISTER is
    record
        F_Ignore_Read    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_IGNORE_READ_FIELD;
        F_Ignore_Write    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_IGNORE_WRITE_FIELD;
        F_Ignore_Write_Ack    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_IGNORE_WRITE_ACK_FIELD;
        F_Ignore_Word_Access    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_IGNORE_WORD_ACCESS_FIELD;
        F_Ignore_Byte_Access    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_IGNORE_BYTE_ACCESS_FIELD;
        F_Ilwert_Addr_Match    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_ILWERT_ADDR_MATCH_FIELD;
        F_Ilwert_Subid_Match    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_ILWERT_SUBID_MATCH_FIELD;
        F_Trap_Application_Level0    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_TRAP_APPLICATION_LEVEL0_FIELD;
        F_Trap_Application_Level1    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_TRAP_APPLICATION_LEVEL1_FIELD;
        F_Trap_Application_Level2    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_TRAP_APPLICATION_LEVEL2_FIELD;
        F_Subid_Ctl    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_SUBID_CTL_FIELD;
        F_Compare_Ctl    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_COMPARE_CTL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_REGISTER use
    record
        F_Ignore_Read at 0 range 4 .. 4;
        F_Ignore_Write at 0 range 5 .. 5;
        F_Ignore_Write_Ack at 0 range 6 .. 6;
        F_Ignore_Word_Access at 0 range 8 .. 8;
        F_Ignore_Byte_Access at 0 range 9 .. 9;
        F_Ilwert_Addr_Match at 0 range 12 .. 12;
        F_Ilwert_Subid_Match at 0 range 13 .. 13;
        F_Trap_Application_Level0 at 0 range 14 .. 14;
        F_Trap_Application_Level1 at 0 range 15 .. 15;
        F_Trap_Application_Level2 at 0 range 16 .. 16;
        F_Subid_Ctl at 0 range 30 .. 30;
        F_Compare_Ctl at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_26B8#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_IGNORE_READ_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Read_Matched,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Read_Ignored
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_IGNORE_READ_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Read_Matched => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Read_Ignored => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_IGNORE_WRITE_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Write_Matched,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Write_Ignored
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_IGNORE_WRITE_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Write_Matched => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Write_Ignored => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_IGNORE_WRITE_ACK_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Write_Ack_Matched,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Write_Ack_Ignored
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_IGNORE_WRITE_ACK_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Write_Ack_Matched => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Write_Ack_Ignored => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_IGNORE_WORD_ACCESS_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Word_Access_Matched,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Word_Access_Ignored
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_IGNORE_WORD_ACCESS_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Word_Access_Matched => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Word_Access_Ignored => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_IGNORE_BYTE_ACCESS_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Byte_Access_Matched,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Byte_Access_Ignored
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_IGNORE_BYTE_ACCESS_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Byte_Access_Matched => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Byte_Access_Ignored => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_ILWERT_ADDR_MATCH_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ilwert_Addr_Match_Normal,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ilwert_Addr_Match_Ilwerted
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_ILWERT_ADDR_MATCH_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ilwert_Addr_Match_Normal => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ilwert_Addr_Match_Ilwerted => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_ILWERT_SUBID_MATCH_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ilwert_Subid_Match_Normal,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ilwert_Subid_Match_Ilwerted
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_ILWERT_SUBID_MATCH_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ilwert_Subid_Match_Normal => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ilwert_Subid_Match_Ilwerted => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_TRAP_APPLICATION_LEVEL0_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Trap_Application_Level0_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Trap_Application_Level0_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_TRAP_APPLICATION_LEVEL0_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Trap_Application_Level0_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Trap_Application_Level0_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_TRAP_APPLICATION_LEVEL1_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Trap_Application_Level1_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Trap_Application_Level1_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_TRAP_APPLICATION_LEVEL1_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Trap_Application_Level1_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Trap_Application_Level1_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_TRAP_APPLICATION_LEVEL2_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Trap_Application_Level2_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Trap_Application_Level2_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_TRAP_APPLICATION_LEVEL2_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Trap_Application_Level2_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Trap_Application_Level2_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_SUBID_CTL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Subid_Ctl_Masked,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Subid_Ctl_Compared
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_SUBID_CTL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Subid_Ctl_Masked => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Subid_Ctl_Compared => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_COMPARE_CTL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Compare_Ctl_Masked,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Compare_Ctl_Compared
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_COMPARE_CTL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Compare_Ctl_Masked => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Compare_Ctl_Compared => 1
    );

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_REGISTER is
    record
        F_Ignore_Read    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_IGNORE_READ_FIELD;
        F_Ignore_Write    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_IGNORE_WRITE_FIELD;
        F_Ignore_Write_Ack    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_IGNORE_WRITE_ACK_FIELD;
        F_Ignore_Word_Access    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_IGNORE_WORD_ACCESS_FIELD;
        F_Ignore_Byte_Access    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_IGNORE_BYTE_ACCESS_FIELD;
        F_Ilwert_Addr_Match    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_ILWERT_ADDR_MATCH_FIELD;
        F_Ilwert_Subid_Match    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_ILWERT_SUBID_MATCH_FIELD;
        F_Trap_Application_Level0    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_TRAP_APPLICATION_LEVEL0_FIELD;
        F_Trap_Application_Level1    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_TRAP_APPLICATION_LEVEL1_FIELD;
        F_Trap_Application_Level2    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_TRAP_APPLICATION_LEVEL2_FIELD;
        F_Subid_Ctl    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_SUBID_CTL_FIELD;
        F_Compare_Ctl    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_COMPARE_CTL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_REGISTER use
    record
        F_Ignore_Read at 0 range 4 .. 4;
        F_Ignore_Write at 0 range 5 .. 5;
        F_Ignore_Write_Ack at 0 range 6 .. 6;
        F_Ignore_Word_Access at 0 range 8 .. 8;
        F_Ignore_Byte_Access at 0 range 9 .. 9;
        F_Ilwert_Addr_Match at 0 range 12 .. 12;
        F_Ilwert_Subid_Match at 0 range 13 .. 13;
        F_Trap_Application_Level0 at 0 range 14 .. 14;
        F_Trap_Application_Level1 at 0 range 15 .. 15;
        F_Trap_Application_Level2 at 0 range 16 .. 16;
        F_Subid_Ctl at 0 range 30 .. 30;
        F_Compare_Ctl at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap12_Mask    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_24B0#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MASK_ADDR_FIELD is new LwU26;
    Lw_Ppriv_Sys_Pri_Decode_Trap12_Mask_Addr_I :
        constant LW_PPRIV_SYS_PRI_DECODE_TRAP12_MASK_ADDR_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MASK_SUBID_FIELD is new LwU6;
    Lw_Ppriv_Sys_Pri_Decode_Trap12_Mask_Subid_I :
        constant LW_PPRIV_SYS_PRI_DECODE_TRAP12_MASK_SUBID_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_MASK_REGISTER is
    record
        F_Addr    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_MASK_ADDR_FIELD;
        F_Subid    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_MASK_SUBID_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_MASK_REGISTER use
    record
        F_Addr at 0 range 0 .. 25;
        F_Subid at 0 range 26 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap13_Mask    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_24B4#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MASK_ADDR_FIELD is new LwU26;
    Lw_Ppriv_Sys_Pri_Decode_Trap13_Mask_Addr_I :
        constant LW_PPRIV_SYS_PRI_DECODE_TRAP13_MASK_ADDR_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MASK_SUBID_FIELD is new LwU6;
    Lw_Ppriv_Sys_Pri_Decode_Trap13_Mask_Subid_I :
        constant LW_PPRIV_SYS_PRI_DECODE_TRAP13_MASK_SUBID_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_MASK_REGISTER is
    record
        F_Addr    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_MASK_ADDR_FIELD;
        F_Subid    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_MASK_SUBID_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_MASK_REGISTER use
    record
        F_Addr at 0 range 0 .. 25;
        F_Subid at 0 range 26 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap14_Mask    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_24B8#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MASK_ADDR_FIELD is new LwU26;
    Lw_Ppriv_Sys_Pri_Decode_Trap14_Mask_Addr_I :
        constant LW_PPRIV_SYS_PRI_DECODE_TRAP14_MASK_ADDR_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MASK_SUBID_FIELD is new LwU6;
    Lw_Ppriv_Sys_Pri_Decode_Trap14_Mask_Subid_I :
        constant LW_PPRIV_SYS_PRI_DECODE_TRAP14_MASK_SUBID_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_MASK_REGISTER is
    record
        F_Addr    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_MASK_ADDR_FIELD;
        F_Subid    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_MASK_SUBID_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_MASK_REGISTER use
    record
        F_Addr at 0 range 0 .. 25;
        F_Subid at 0 range 26 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_2530#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1_V_RS_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_All,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_All_Gpc,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_All_Fbp,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_All_Sys,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Sys0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Sys1,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Fbp0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Fbp1,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Fbp2,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Fbp3,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Fbp4,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Fbp5,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Gpc0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Gpc1,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Gpc2,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Gpc3,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Gpc4,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Gpc5
    ) with size => 6;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1_V_RS_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_All => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_All_Gpc => 1,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_All_Fbp => 2,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_All_Sys => 3,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Sys0 => 8,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Sys1 => 9,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Fbp0 => 16,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Fbp1 => 17,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Fbp2 => 18,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Fbp3 => 19,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Fbp4 => 20,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Fbp5 => 21,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Gpc0 => 32,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Gpc1 => 33,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Gpc2 => 34,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Gpc3 => 35,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Gpc4 => 36,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_Gpc5 => 37
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1_V_MC_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Mc_Bc,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Mc_Mc0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Mc_Mc1,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Mc_Mc2
    ) with size => 2;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1_V_MC_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Mc_Bc => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Mc_Mc0 => 1,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Mc_Mc1 => 2,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Mc_Mc2 => 3
    );

   type LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1_PADDING_FIELD is
     (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_Padding_0
     ) with size => 22;
    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1_PADDING_FIELD use
     (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_Padding_0 => 0
     );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1_V_FOR_ACTION_SET_PRIV_LEVEL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_For_Action_Set_Priv_Level_Value_Level_0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_For_Action_Set_Priv_Level_Value_Level_1,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_For_Action_Set_Priv_Level_Value_Level_2,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_For_Action_Set_Priv_Level_Value_Level_3
    ) with size => 2;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1_V_FOR_ACTION_SET_PRIV_LEVEL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_For_Action_Set_Priv_Level_Value_Level_0 => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_For_Action_Set_Priv_Level_Value_Level_1 => 1,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_For_Action_Set_Priv_Level_Value_Level_2 => 2,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_For_Action_Set_Priv_Level_Value_Level_3 => 3
    );

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1_REGISTER is
    record
        F_V_Rs    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1_V_RS_FIELD;
        F_V_Mc    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1_V_MC_FIELD;
        F_PAD     : LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1_PADDING_FIELD;
        F_V_For_Action_Set_Priv_Level    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1_V_FOR_ACTION_SET_PRIV_LEVEL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1_REGISTER use
    record
        F_V_Rs at 0 range 0 .. 5;
        F_V_Mc at 0 range 6 .. 7;
        F_PAD at 0 range  8 .. 29;
        F_V_For_Action_Set_Priv_Level at 0 range 30 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_2534#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1_V_RS_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_All,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_All_Gpc,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_All_Fbp,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_All_Sys,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Sys0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Sys1,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Fbp0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Fbp1,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Fbp2,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Fbp3,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Fbp4,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Fbp5,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Gpc0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Gpc1,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Gpc2,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Gpc3,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Gpc4,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Gpc5
    ) with size => 6;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1_V_RS_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_All => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_All_Gpc => 1,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_All_Fbp => 2,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_All_Sys => 3,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Sys0 => 8,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Sys1 => 9,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Fbp0 => 16,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Fbp1 => 17,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Fbp2 => 18,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Fbp3 => 19,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Fbp4 => 20,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Fbp5 => 21,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Gpc0 => 32,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Gpc1 => 33,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Gpc2 => 34,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Gpc3 => 35,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Gpc4 => 36,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_Gpc5 => 37
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1_V_MC_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Mc_Bc,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Mc_Mc0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Mc_Mc1,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Mc_Mc2
    ) with size => 2;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1_V_MC_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Mc_Bc => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Mc_Mc0 => 1,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Mc_Mc1 => 2,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Mc_Mc2 => 3
    );

   type LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1_PADDING_FIELD is
     (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_Padding_0
     ) with size => 22;
   for LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1_PADDING_FIELD use
     (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_Padding_0 => 0
     );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1_V_FOR_ACTION_SET_PRIV_LEVEL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_For_Action_Set_Priv_Level_Value_Level_0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_For_Action_Set_Priv_Level_Value_Level_1,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_For_Action_Set_Priv_Level_Value_Level_2,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_For_Action_Set_Priv_Level_Value_Level_3
    ) with size => 2;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1_V_FOR_ACTION_SET_PRIV_LEVEL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_For_Action_Set_Priv_Level_Value_Level_0 => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_For_Action_Set_Priv_Level_Value_Level_1 => 1,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_For_Action_Set_Priv_Level_Value_Level_2 => 2,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_For_Action_Set_Priv_Level_Value_Level_3 => 3
    );

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1_REGISTER is
    record
        F_V_Rs    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1_V_RS_FIELD;
        F_V_Mc    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1_V_MC_FIELD;
        F_PAD     : LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1_PADDING_FIELD;
        F_V_For_Action_Set_Priv_Level    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1_V_FOR_ACTION_SET_PRIV_LEVEL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1_REGISTER use
    record
        F_V_Rs at 0 range 0 .. 5;
        F_V_Mc at 0 range 6 .. 7;
        F_PAD at 0 range 8 .. 29;
        F_V_For_Action_Set_Priv_Level at 0 range 30 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_2538#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1_V_RS_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_All,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_All_Gpc,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_All_Fbp,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_All_Sys,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Sys0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Sys1,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Fbp0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Fbp1,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Fbp2,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Fbp3,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Fbp4,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Fbp5,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Gpc0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Gpc1,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Gpc2,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Gpc3,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Gpc4,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Gpc5
    ) with size => 6;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1_V_RS_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_All => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_All_Gpc => 1,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_All_Fbp => 2,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_All_Sys => 3,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Sys0 => 8,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Sys1 => 9,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Fbp0 => 16,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Fbp1 => 17,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Fbp2 => 18,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Fbp3 => 19,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Fbp4 => 20,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Fbp5 => 21,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Gpc0 => 32,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Gpc1 => 33,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Gpc2 => 34,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Gpc3 => 35,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Gpc4 => 36,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_Gpc5 => 37
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1_V_MC_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Mc_Bc,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Mc_Mc0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Mc_Mc1,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Mc_Mc2
    ) with size => 2;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1_V_MC_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Mc_Bc => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Mc_Mc0 => 1,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Mc_Mc1 => 2,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Mc_Mc2 => 3
    );

   type LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1_PADDING_FIELD is
     (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_Padding_0
     ) with size => 22;
   for LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1_PADDING_FIELD use
     (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_Padding_0 => 0
     );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1_V_FOR_ACTION_SET_PRIV_LEVEL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_For_Action_Set_Priv_Level_Value_Level_0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_For_Action_Set_Priv_Level_Value_Level_1,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_For_Action_Set_Priv_Level_Value_Level_2,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_For_Action_Set_Priv_Level_Value_Level_3
    ) with size => 2;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1_V_FOR_ACTION_SET_PRIV_LEVEL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_For_Action_Set_Priv_Level_Value_Level_0 => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_For_Action_Set_Priv_Level_Value_Level_1 => 1,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_For_Action_Set_Priv_Level_Value_Level_2 => 2,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_For_Action_Set_Priv_Level_Value_Level_3 => 3
    );

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1_REGISTER is
    record
        F_V_Rs    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1_V_RS_FIELD;
        F_V_Mc    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1_V_MC_FIELD;
        F_PAD     : LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1_PADDING_FIELD;
        F_V_For_Action_Set_Priv_Level    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1_V_FOR_ACTION_SET_PRIV_LEVEL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1_REGISTER use
    record
        F_V_Rs at 0 range 0 .. 5;
        F_V_Mc at 0 range 6 .. 7;
        F_PAD at 0 range 8 .. 29;
        F_V_For_Action_Set_Priv_Level at 0 range 30 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap12_Data2    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_25B0#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA2_V_FIELD is new LwU32;
    Lw_Ppriv_Sys_Pri_Decode_Trap12_Data2_V_I :
        constant LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA2_V_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA2_REGISTER is
    record
        F_V    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA2_V_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA2_REGISTER use
    record
        F_V at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap13_Data2    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_25B4#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA2_V_FIELD is new LwU32;
    Lw_Ppriv_Sys_Pri_Decode_Trap13_Data2_V_I :
        constant LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA2_V_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA2_REGISTER is
    record
        F_V    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA2_V_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA2_REGISTER use
    record
        F_V at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap14_Data2    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_25B8#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA2_V_FIELD is new LwU32;
    Lw_Ppriv_Sys_Pri_Decode_Trap14_Data2_V_I :
        constant LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA2_V_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA2_REGISTER is
    record
        F_V    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA2_V_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA2_REGISTER use
    record
        F_V at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap12_Action    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_2630#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_DROP_TRANSACTION_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Drop_Transaction_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Drop_Transaction_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_DROP_TRANSACTION_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Drop_Transaction_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Drop_Transaction_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_FORCE_DECODE_PHYSICAL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Force_Decode_Physical_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Force_Decode_Physical_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_FORCE_DECODE_PHYSICAL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Force_Decode_Physical_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Force_Decode_Physical_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_FORCE_DECODE_VIRTUAL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Force_Decode_Virtual_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Force_Decode_Virtual_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_FORCE_DECODE_VIRTUAL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Force_Decode_Virtual_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Force_Decode_Virtual_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_FORCE_ERROR_RETURN_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Force_Error_Return_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Force_Error_Return_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_FORCE_ERROR_RETURN_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Force_Error_Return_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Force_Error_Return_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_RETURN_SPECIFIED_DATA_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Return_Specified_Data_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Return_Specified_Data_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_RETURN_SPECIFIED_DATA_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Return_Specified_Data_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Return_Specified_Data_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_LOG_REQUEST_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Log_Request_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Log_Request_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_LOG_REQUEST_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Log_Request_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Log_Request_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_REDIRECT_TO_ADDRESS_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Redirect_To_Address_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Redirect_To_Address_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_REDIRECT_TO_ADDRESS_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Redirect_To_Address_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Redirect_To_Address_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_REDIRECT_TO_RINGSTATION_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Redirect_To_Ringstation_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Redirect_To_Ringstation_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_REDIRECT_TO_RINGSTATION_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Redirect_To_Ringstation_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Redirect_To_Ringstation_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_OVERRIDE_ACK_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Ack_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Ack_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_OVERRIDE_ACK_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Ack_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Ack_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_OVERRIDE_WITHOUT_ACK_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Without_Ack_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Without_Ack_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_OVERRIDE_WITHOUT_ACK_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Without_Ack_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Without_Ack_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_BC_READ_AND_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Bc_Read_And_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Bc_Read_And_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_BC_READ_AND_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Bc_Read_And_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Bc_Read_And_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_BC_READ_OR_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Bc_Read_Or_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Bc_Read_Or_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_BC_READ_OR_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Bc_Read_Or_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Bc_Read_Or_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_OVERRIDE_DECODE_ERROR_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Decode_Error_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Decode_Error_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_OVERRIDE_DECODE_ERROR_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Decode_Error_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Decode_Error_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_SET_LOCAL_ORDERING_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Set_Local_Ordering_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Set_Local_Ordering_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_SET_LOCAL_ORDERING_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Set_Local_Ordering_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Set_Local_Ordering_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_CLEAR_LOCAL_ORDERING_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Clear_Local_Ordering_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Clear_Local_Ordering_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_CLEAR_LOCAL_ORDERING_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Clear_Local_Ordering_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Clear_Local_Ordering_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_UCODE_WRITE_INTERCEPT_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Write_Intercept_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Write_Intercept_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_UCODE_WRITE_INTERCEPT_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Write_Intercept_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Write_Intercept_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_UCODE_READ_INTERCEPT_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Read_Intercept_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Read_Intercept_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_UCODE_READ_INTERCEPT_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Read_Intercept_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Read_Intercept_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_UCODE_WRITE_OBSERVE_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Write_Observe_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Write_Observe_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_UCODE_WRITE_OBSERVE_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Write_Observe_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Write_Observe_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_UCODE_READ_OBSERVE_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Read_Observe_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Read_Observe_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_UCODE_READ_OBSERVE_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Read_Observe_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Read_Observe_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_SET_PRIV_LEVEL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Set_Priv_Level_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Set_Priv_Level_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_SET_PRIV_LEVEL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Set_Priv_Level_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Set_Priv_Level_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_OVERRIDE_WRBE_ERROR_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Wrbe_Error_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Wrbe_Error_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_OVERRIDE_WRBE_ERROR_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Wrbe_Error_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Wrbe_Error_Enable => 1
    );

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_REGISTER is
    record
        F_Drop_Transaction    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_DROP_TRANSACTION_FIELD;
        F_Force_Decode_Physical    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_FORCE_DECODE_PHYSICAL_FIELD;
        F_Force_Decode_Virtual    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_FORCE_DECODE_VIRTUAL_FIELD;
        F_Force_Error_Return    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_FORCE_ERROR_RETURN_FIELD;
        F_Return_Specified_Data    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_RETURN_SPECIFIED_DATA_FIELD;
        F_Log_Request    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_LOG_REQUEST_FIELD;
        F_Redirect_To_Address    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_REDIRECT_TO_ADDRESS_FIELD;
        F_Redirect_To_Ringstation    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_REDIRECT_TO_RINGSTATION_FIELD;
        F_Override_Ack    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_OVERRIDE_ACK_FIELD;
        F_Override_Without_Ack    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_OVERRIDE_WITHOUT_ACK_FIELD;
        F_Bc_Read_And    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_BC_READ_AND_FIELD;
        F_Bc_Read_Or    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_BC_READ_OR_FIELD;
        F_Override_Decode_Error    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_OVERRIDE_DECODE_ERROR_FIELD;
        F_Set_Local_Ordering    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_SET_LOCAL_ORDERING_FIELD;
        F_Clear_Local_Ordering    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_CLEAR_LOCAL_ORDERING_FIELD;
        F_Ucode_Write_Intercept    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_UCODE_WRITE_INTERCEPT_FIELD;
        F_Ucode_Read_Intercept    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_UCODE_READ_INTERCEPT_FIELD;
        F_Ucode_Write_Observe    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_UCODE_WRITE_OBSERVE_FIELD;
        F_Ucode_Read_Observe    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_UCODE_READ_OBSERVE_FIELD;
        F_Set_Priv_Level    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_SET_PRIV_LEVEL_FIELD;
        F_Override_Wrbe_Error    : LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_OVERRIDE_WRBE_ERROR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_REGISTER use
    record
        F_Drop_Transaction at 0 range 0 .. 0;
        F_Force_Decode_Physical at 0 range 1 .. 1;
        F_Force_Decode_Virtual at 0 range 2 .. 2;
        F_Force_Error_Return at 0 range 3 .. 3;
        F_Return_Specified_Data at 0 range 4 .. 4;
        F_Log_Request at 0 range 5 .. 5;
        F_Redirect_To_Address at 0 range 6 .. 6;
        F_Redirect_To_Ringstation at 0 range 7 .. 7;
        F_Override_Ack at 0 range 9 .. 9;
        F_Override_Without_Ack at 0 range 10 .. 10;
        F_Bc_Read_And at 0 range 11 .. 11;
        F_Bc_Read_Or at 0 range 12 .. 12;
        F_Override_Decode_Error at 0 range 13 .. 13;
        F_Set_Local_Ordering at 0 range 14 .. 14;
        F_Clear_Local_Ordering at 0 range 15 .. 15;
        F_Ucode_Write_Intercept at 0 range 16 .. 16;
        F_Ucode_Read_Intercept at 0 range 17 .. 17;
        F_Ucode_Write_Observe at 0 range 18 .. 18;
        F_Ucode_Read_Observe at 0 range 19 .. 19;
        F_Set_Priv_Level at 0 range 20 .. 20;
        F_Override_Wrbe_Error at 0 range 21 .. 21;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap13_Action    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_2634#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_DROP_TRANSACTION_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Drop_Transaction_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Drop_Transaction_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_DROP_TRANSACTION_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Drop_Transaction_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Drop_Transaction_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_FORCE_DECODE_PHYSICAL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Force_Decode_Physical_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Force_Decode_Physical_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_FORCE_DECODE_PHYSICAL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Force_Decode_Physical_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Force_Decode_Physical_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_FORCE_DECODE_VIRTUAL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Force_Decode_Virtual_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Force_Decode_Virtual_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_FORCE_DECODE_VIRTUAL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Force_Decode_Virtual_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Force_Decode_Virtual_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_FORCE_ERROR_RETURN_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Force_Error_Return_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Force_Error_Return_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_FORCE_ERROR_RETURN_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Force_Error_Return_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Force_Error_Return_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_RETURN_SPECIFIED_DATA_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Return_Specified_Data_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Return_Specified_Data_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_RETURN_SPECIFIED_DATA_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Return_Specified_Data_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Return_Specified_Data_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_LOG_REQUEST_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Log_Request_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Log_Request_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_LOG_REQUEST_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Log_Request_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Log_Request_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_REDIRECT_TO_ADDRESS_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Redirect_To_Address_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Redirect_To_Address_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_REDIRECT_TO_ADDRESS_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Redirect_To_Address_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Redirect_To_Address_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_REDIRECT_TO_RINGSTATION_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Redirect_To_Ringstation_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Redirect_To_Ringstation_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_REDIRECT_TO_RINGSTATION_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Redirect_To_Ringstation_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Redirect_To_Ringstation_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_OVERRIDE_ACK_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Ack_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Ack_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_OVERRIDE_ACK_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Ack_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Ack_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_OVERRIDE_WITHOUT_ACK_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Without_Ack_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Without_Ack_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_OVERRIDE_WITHOUT_ACK_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Without_Ack_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Without_Ack_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_BC_READ_AND_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Bc_Read_And_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Bc_Read_And_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_BC_READ_AND_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Bc_Read_And_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Bc_Read_And_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_BC_READ_OR_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Bc_Read_Or_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Bc_Read_Or_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_BC_READ_OR_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Bc_Read_Or_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Bc_Read_Or_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_OVERRIDE_DECODE_ERROR_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Decode_Error_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Decode_Error_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_OVERRIDE_DECODE_ERROR_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Decode_Error_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Decode_Error_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_SET_LOCAL_ORDERING_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Set_Local_Ordering_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Set_Local_Ordering_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_SET_LOCAL_ORDERING_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Set_Local_Ordering_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Set_Local_Ordering_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_CLEAR_LOCAL_ORDERING_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Clear_Local_Ordering_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Clear_Local_Ordering_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_CLEAR_LOCAL_ORDERING_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Clear_Local_Ordering_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Clear_Local_Ordering_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_UCODE_WRITE_INTERCEPT_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Write_Intercept_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Write_Intercept_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_UCODE_WRITE_INTERCEPT_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Write_Intercept_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Write_Intercept_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_UCODE_READ_INTERCEPT_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Read_Intercept_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Read_Intercept_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_UCODE_READ_INTERCEPT_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Read_Intercept_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Read_Intercept_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_UCODE_WRITE_OBSERVE_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Write_Observe_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Write_Observe_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_UCODE_WRITE_OBSERVE_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Write_Observe_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Write_Observe_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_UCODE_READ_OBSERVE_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Read_Observe_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Read_Observe_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_UCODE_READ_OBSERVE_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Read_Observe_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Read_Observe_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_SET_PRIV_LEVEL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Set_Priv_Level_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Set_Priv_Level_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_SET_PRIV_LEVEL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Set_Priv_Level_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Set_Priv_Level_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_OVERRIDE_WRBE_ERROR_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Wrbe_Error_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Wrbe_Error_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_OVERRIDE_WRBE_ERROR_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Wrbe_Error_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Wrbe_Error_Enable => 1
    );

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_REGISTER is
    record
        F_Drop_Transaction    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_DROP_TRANSACTION_FIELD;
        F_Force_Decode_Physical    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_FORCE_DECODE_PHYSICAL_FIELD;
        F_Force_Decode_Virtual    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_FORCE_DECODE_VIRTUAL_FIELD;
        F_Force_Error_Return    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_FORCE_ERROR_RETURN_FIELD;
        F_Return_Specified_Data    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_RETURN_SPECIFIED_DATA_FIELD;
        F_Log_Request    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_LOG_REQUEST_FIELD;
        F_Redirect_To_Address    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_REDIRECT_TO_ADDRESS_FIELD;
        F_Redirect_To_Ringstation    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_REDIRECT_TO_RINGSTATION_FIELD;
        F_Override_Ack    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_OVERRIDE_ACK_FIELD;
        F_Override_Without_Ack    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_OVERRIDE_WITHOUT_ACK_FIELD;
        F_Bc_Read_And    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_BC_READ_AND_FIELD;
        F_Bc_Read_Or    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_BC_READ_OR_FIELD;
        F_Override_Decode_Error    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_OVERRIDE_DECODE_ERROR_FIELD;
        F_Set_Local_Ordering    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_SET_LOCAL_ORDERING_FIELD;
        F_Clear_Local_Ordering    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_CLEAR_LOCAL_ORDERING_FIELD;
        F_Ucode_Write_Intercept    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_UCODE_WRITE_INTERCEPT_FIELD;
        F_Ucode_Read_Intercept    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_UCODE_READ_INTERCEPT_FIELD;
        F_Ucode_Write_Observe    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_UCODE_WRITE_OBSERVE_FIELD;
        F_Ucode_Read_Observe    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_UCODE_READ_OBSERVE_FIELD;
        F_Set_Priv_Level    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_SET_PRIV_LEVEL_FIELD;
        F_Override_Wrbe_Error    : LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_OVERRIDE_WRBE_ERROR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_REGISTER use
    record
        F_Drop_Transaction at 0 range 0 .. 0;
        F_Force_Decode_Physical at 0 range 1 .. 1;
        F_Force_Decode_Virtual at 0 range 2 .. 2;
        F_Force_Error_Return at 0 range 3 .. 3;
        F_Return_Specified_Data at 0 range 4 .. 4;
        F_Log_Request at 0 range 5 .. 5;
        F_Redirect_To_Address at 0 range 6 .. 6;
        F_Redirect_To_Ringstation at 0 range 7 .. 7;
        F_Override_Ack at 0 range 9 .. 9;
        F_Override_Without_Ack at 0 range 10 .. 10;
        F_Bc_Read_And at 0 range 11 .. 11;
        F_Bc_Read_Or at 0 range 12 .. 12;
        F_Override_Decode_Error at 0 range 13 .. 13;
        F_Set_Local_Ordering at 0 range 14 .. 14;
        F_Clear_Local_Ordering at 0 range 15 .. 15;
        F_Ucode_Write_Intercept at 0 range 16 .. 16;
        F_Ucode_Read_Intercept at 0 range 17 .. 17;
        F_Ucode_Write_Observe at 0 range 18 .. 18;
        F_Ucode_Read_Observe at 0 range 19 .. 19;
        F_Set_Priv_Level at 0 range 20 .. 20;
        F_Override_Wrbe_Error at 0 range 21 .. 21;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppriv_Sys_Pri_Decode_Trap14_Action    : constant LW_PPRIV_SYS_BAR0_ADDR := 16#0012_2638#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_DROP_TRANSACTION_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Drop_Transaction_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Drop_Transaction_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_DROP_TRANSACTION_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Drop_Transaction_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Drop_Transaction_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_FORCE_DECODE_PHYSICAL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Force_Decode_Physical_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Force_Decode_Physical_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_FORCE_DECODE_PHYSICAL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Force_Decode_Physical_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Force_Decode_Physical_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_FORCE_DECODE_VIRTUAL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Force_Decode_Virtual_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Force_Decode_Virtual_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_FORCE_DECODE_VIRTUAL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Force_Decode_Virtual_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Force_Decode_Virtual_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_FORCE_ERROR_RETURN_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Force_Error_Return_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Force_Error_Return_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_FORCE_ERROR_RETURN_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Force_Error_Return_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Force_Error_Return_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_RETURN_SPECIFIED_DATA_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Return_Specified_Data_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Return_Specified_Data_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_RETURN_SPECIFIED_DATA_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Return_Specified_Data_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Return_Specified_Data_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_LOG_REQUEST_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Log_Request_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Log_Request_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_LOG_REQUEST_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Log_Request_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Log_Request_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_REDIRECT_TO_ADDRESS_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Redirect_To_Address_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Redirect_To_Address_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_REDIRECT_TO_ADDRESS_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Redirect_To_Address_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Redirect_To_Address_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_REDIRECT_TO_RINGSTATION_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Redirect_To_Ringstation_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Redirect_To_Ringstation_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_REDIRECT_TO_RINGSTATION_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Redirect_To_Ringstation_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Redirect_To_Ringstation_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_OVERRIDE_ACK_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Ack_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Ack_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_OVERRIDE_ACK_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Ack_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Ack_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_OVERRIDE_WITHOUT_ACK_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Without_Ack_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Without_Ack_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_OVERRIDE_WITHOUT_ACK_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Without_Ack_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Without_Ack_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_BC_READ_AND_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Bc_Read_And_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Bc_Read_And_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_BC_READ_AND_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Bc_Read_And_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Bc_Read_And_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_BC_READ_OR_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Bc_Read_Or_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Bc_Read_Or_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_BC_READ_OR_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Bc_Read_Or_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Bc_Read_Or_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_OVERRIDE_DECODE_ERROR_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Decode_Error_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Decode_Error_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_OVERRIDE_DECODE_ERROR_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Decode_Error_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Decode_Error_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_SET_LOCAL_ORDERING_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Set_Local_Ordering_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Set_Local_Ordering_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_SET_LOCAL_ORDERING_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Set_Local_Ordering_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Set_Local_Ordering_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_CLEAR_LOCAL_ORDERING_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Clear_Local_Ordering_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Clear_Local_Ordering_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_CLEAR_LOCAL_ORDERING_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Clear_Local_Ordering_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Clear_Local_Ordering_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_UCODE_WRITE_INTERCEPT_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Write_Intercept_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Write_Intercept_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_UCODE_WRITE_INTERCEPT_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Write_Intercept_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Write_Intercept_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_UCODE_READ_INTERCEPT_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Read_Intercept_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Read_Intercept_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_UCODE_READ_INTERCEPT_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Read_Intercept_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Read_Intercept_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_UCODE_WRITE_OBSERVE_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Write_Observe_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Write_Observe_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_UCODE_WRITE_OBSERVE_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Write_Observe_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Write_Observe_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_UCODE_READ_OBSERVE_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Read_Observe_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Read_Observe_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_UCODE_READ_OBSERVE_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Read_Observe_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Read_Observe_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_SET_PRIV_LEVEL_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Set_Priv_Level_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Set_Priv_Level_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_SET_PRIV_LEVEL_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Set_Priv_Level_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Set_Priv_Level_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_OVERRIDE_WRBE_ERROR_FIELD is
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Wrbe_Error_Disable,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Wrbe_Error_Enable
    ) with size => 1;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_OVERRIDE_WRBE_ERROR_FIELD use
    (
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Wrbe_Error_Disable => 0,
        Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Wrbe_Error_Enable => 1
    );

---------- Record Declaration ----------

    type LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_REGISTER is
    record
        F_Drop_Transaction    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_DROP_TRANSACTION_FIELD;
        F_Force_Decode_Physical    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_FORCE_DECODE_PHYSICAL_FIELD;
        F_Force_Decode_Virtual    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_FORCE_DECODE_VIRTUAL_FIELD;
        F_Force_Error_Return    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_FORCE_ERROR_RETURN_FIELD;
        F_Return_Specified_Data    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_RETURN_SPECIFIED_DATA_FIELD;
        F_Log_Request    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_LOG_REQUEST_FIELD;
        F_Redirect_To_Address    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_REDIRECT_TO_ADDRESS_FIELD;
        F_Redirect_To_Ringstation    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_REDIRECT_TO_RINGSTATION_FIELD;
        F_Override_Ack    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_OVERRIDE_ACK_FIELD;
        F_Override_Without_Ack    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_OVERRIDE_WITHOUT_ACK_FIELD;
        F_Bc_Read_And    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_BC_READ_AND_FIELD;
        F_Bc_Read_Or    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_BC_READ_OR_FIELD;
        F_Override_Decode_Error    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_OVERRIDE_DECODE_ERROR_FIELD;
        F_Set_Local_Ordering    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_SET_LOCAL_ORDERING_FIELD;
        F_Clear_Local_Ordering    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_CLEAR_LOCAL_ORDERING_FIELD;
        F_Ucode_Write_Intercept    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_UCODE_WRITE_INTERCEPT_FIELD;
        F_Ucode_Read_Intercept    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_UCODE_READ_INTERCEPT_FIELD;
        F_Ucode_Write_Observe    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_UCODE_WRITE_OBSERVE_FIELD;
        F_Ucode_Read_Observe    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_UCODE_READ_OBSERVE_FIELD;
        F_Set_Priv_Level    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_SET_PRIV_LEVEL_FIELD;
        F_Override_Wrbe_Error    : LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_OVERRIDE_WRBE_ERROR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_REGISTER use
    record
        F_Drop_Transaction at 0 range 0 .. 0;
        F_Force_Decode_Physical at 0 range 1 .. 1;
        F_Force_Decode_Virtual at 0 range 2 .. 2;
        F_Force_Error_Return at 0 range 3 .. 3;
        F_Return_Specified_Data at 0 range 4 .. 4;
        F_Log_Request at 0 range 5 .. 5;
        F_Redirect_To_Address at 0 range 6 .. 6;
        F_Redirect_To_Ringstation at 0 range 7 .. 7;
        F_Override_Ack at 0 range 9 .. 9;
        F_Override_Without_Ack at 0 range 10 .. 10;
        F_Bc_Read_And at 0 range 11 .. 11;
        F_Bc_Read_Or at 0 range 12 .. 12;
        F_Override_Decode_Error at 0 range 13 .. 13;
        F_Set_Local_Ordering at 0 range 14 .. 14;
        F_Clear_Local_Ordering at 0 range 15 .. 15;
        F_Ucode_Write_Intercept at 0 range 16 .. 16;
        F_Ucode_Read_Intercept at 0 range 17 .. 17;
        F_Ucode_Write_Observe at 0 range 18 .. 18;
        F_Ucode_Read_Observe at 0 range 19 .. 19;
        F_Set_Priv_Level at 0 range 20 .. 20;
        F_Override_Wrbe_Error at 0 range 21 .. 21;
    end record;

------------------------------------------------------------------------------------------------------
end Dev_Pri_Ringstation_Sys_Ada;
