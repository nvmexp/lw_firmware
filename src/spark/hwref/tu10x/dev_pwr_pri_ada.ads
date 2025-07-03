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

--******************************  DEV_PWR_PRI  ******************************--

package Dev_Pwr_Pri_Ada
with SPARK_MODE => On
is


---------- Register Range Subtype Declaration ----------
    subtype LW_PPWR_BAR0_ADDR is BAR0_ADDR range 16#0010_A000# .. 16#0010_BFFF#;
------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask    : constant LW_PPWR_BAR0_ADDR := 16#0010_A284#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Record Declaration ----------

    type LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Read_Violation    : LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Protection    : LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Write_Violation    : LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 2;
        F_Read_Violation at 0 range 3 .. 3;
        F_Write_Protection at 0 range 4 .. 6;
        F_Write_Violation at 0 range 7 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppwr_Falcon_Exe_Priv_Level_Mask    : constant LW_PPWR_BAR0_ADDR := 16#0010_A28C#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Record Declaration ----------

    type LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Read_Violation    : LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Protection    : LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Write_Violation    : LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 2;
        F_Read_Violation at 0 range 3 .. 3;
        F_Write_Protection at 0 range 4 .. 6;
        F_Write_Violation at 0 range 7 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask    : constant LW_PPWR_BAR0_ADDR := 16#0010_A290#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Record Declaration ----------

    type LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Read_Violation    : LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Protection    : LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Write_Violation    : LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 2;
        F_Read_Violation at 0 range 3 .. 3;
        F_Write_Protection at 0 range 4 .. 6;
        F_Write_Violation at 0 range 7 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask    : constant LW_PPWR_BAR0_ADDR := 16#0010_A298#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_FIELD is
    (
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Protection_Level0_Disable,
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Protection_Level0_Enable
    ) with size => 1;

    for LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_FIELD use
    (
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Protection_Level0_Disable => 0,
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Protection_Level0_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL1_FIELD is
    (
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Protection_Level1_Disable,
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Protection_Level1_Enable
    ) with size => 1;

    for LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL1_FIELD use
    (
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Protection_Level1_Disable => 0,
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Protection_Level1_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL2_FIELD is
    (
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Protection_Level2_Disable,
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Protection_Level2_Enable
    ) with size => 1;

    for LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL2_FIELD use
    (
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Protection_Level2_Disable => 0,
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Protection_Level2_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Record Declaration ----------

    type LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Read_Violation    : LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Protection_Level0    : LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_FIELD;
        F_Write_Protection_Level1    : LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL1_FIELD;
        F_Write_Protection_Level2    : LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL2_FIELD;
        F_Write_Violation    : LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 2;
        F_Read_Violation at 0 range 3 .. 3;
        F_Write_Protection_Level0 at 0 range 4 .. 4;
        F_Write_Protection_Level1 at 0 range 5 .. 5;
        F_Write_Protection_Level2 at 0 range 6 .. 6;
        F_Write_Violation at 0 range 7 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask    : constant LW_PPWR_BAR0_ADDR := 16#0010_A29C#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Record Declaration ----------

    type LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Read_Violation    : LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Protection    : LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Write_Violation    : LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 2;
        F_Read_Violation at 0 range 3 .. 3;
        F_Write_Protection at 0 range 4 .. 6;
        F_Write_Violation at 0 range 7 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask    : constant LW_PPWR_BAR0_ADDR := 16#0010_BF78#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Read_Protection_Level2_Enabled,
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Read_Protection_Level2_Enabled => 12,
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Write_Protection_Level2_Enabled,
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Write_Protection_Level2_Enabled => 12,
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask    : constant LW_PPWR_BAR0_ADDR := 16#0010_BF7C#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Read_Protection_Level2_Enabled,
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Read_Protection_Level2_Enabled => 12,
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Write_Protection_Level2_Enabled,
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Write_Protection_Level2_Enabled => 12,
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask    : constant LW_PPWR_BAR0_ADDR := 16#0010_AE70#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Record Declaration ----------

    type LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Read_Violation    : LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Protection    : LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Write_Violation    : LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 2;
        F_Read_Violation at 0 range 3 .. 3;
        F_Write_Protection at 0 range 4 .. 6;
        F_Write_Violation at 0 range 7 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppwr_Falcon_Engine    : constant LW_PPWR_BAR0_ADDR := 16#0010_A3C0#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPWR_FALCON_ENGINE_RESET_FIELD is
    (
        Lw_Ppwr_Falcon_Engine_Reset_False,
        Lw_Ppwr_Falcon_Engine_Reset_True
    ) with size => 1;

    for LW_PPWR_FALCON_ENGINE_RESET_FIELD use
    (
        Lw_Ppwr_Falcon_Engine_Reset_False => 0,
        Lw_Ppwr_Falcon_Engine_Reset_True => 1
    );

---------- Record Declaration ----------

    type LW_PPWR_FALCON_ENGINE_REGISTER is
    record
        F_Reset    : LW_PPWR_FALCON_ENGINE_RESET_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPWR_FALCON_ENGINE_REGISTER use
    record
        F_Reset at 0 range 0 .. 0;
    end record;

   ------------------------------------------------------------------------------------------------------

   ------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppwr_Falcon_Sctl    : constant LW_PPWR_BAR0_ADDR := 16#0010_A240#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPWR_FALCON_SCTL_LSMODE_FIELD is
    (
        Lw_Ppwr_Falcon_Sctl_Lsmode_False,
        Lw_Ppwr_Falcon_Sctl_Lsmode_True
    ) with size => 1;

    for LW_PPWR_FALCON_SCTL_LSMODE_FIELD use
    (
        Lw_Ppwr_Falcon_Sctl_Lsmode_False => 0,
        Lw_Ppwr_Falcon_Sctl_Lsmode_True => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_SCTL_HSMODE_FIELD is
    (
        Lw_Ppwr_Falcon_Sctl_Hsmode_False,
        Lw_Ppwr_Falcon_Sctl_Hsmode_True
    ) with size => 1;

    for LW_PPWR_FALCON_SCTL_HSMODE_FIELD use
    (
        Lw_Ppwr_Falcon_Sctl_Hsmode_False => 0,
        Lw_Ppwr_Falcon_Sctl_Hsmode_True => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PPWR_FALCON_SCTL_LSMODE_LEVEL_FIELD is new LwU2;
    Lw_Ppwr_Falcon_Sctl_Lsmode_Level_Init :
        constant LW_PPWR_FALCON_SCTL_LSMODE_LEVEL_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PPWR_FALCON_SCTL_DEBUG_PRIV_LEVEL_FIELD is new LwU2;
    Lw_Ppwr_Falcon_Sctl_Debug_Priv_Level_Init :
        constant LW_PPWR_FALCON_SCTL_DEBUG_PRIV_LEVEL_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_SCTL_RESET_LVLM_EN_FIELD is
    (
        Lw_Ppwr_Falcon_Sctl_Reset_Lvlm_En_False,
        Lw_Ppwr_Falcon_Sctl_Reset_Lvlm_En_True
    ) with size => 1;

    for LW_PPWR_FALCON_SCTL_RESET_LVLM_EN_FIELD use
    (
        Lw_Ppwr_Falcon_Sctl_Reset_Lvlm_En_False => 0,
        Lw_Ppwr_Falcon_Sctl_Reset_Lvlm_En_True => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_SCTL_STALLREQ_CLR_EN_FIELD is
    (
        Lw_Ppwr_Falcon_Sctl_Stallreq_Clr_En_False,
        Lw_Ppwr_Falcon_Sctl_Stallreq_Clr_En_True
    ) with size => 1;

    for LW_PPWR_FALCON_SCTL_STALLREQ_CLR_EN_FIELD use
    (
        Lw_Ppwr_Falcon_Sctl_Stallreq_Clr_En_False => 0,
        Lw_Ppwr_Falcon_Sctl_Stallreq_Clr_En_True => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_SCTL_AUTH_EN_FIELD is
    (
        Lw_Ppwr_Falcon_Sctl_Auth_En_False,
        Lw_Ppwr_Falcon_Sctl_Auth_En_True
    ) with size => 1;

    for LW_PPWR_FALCON_SCTL_AUTH_EN_FIELD use
    (
        Lw_Ppwr_Falcon_Sctl_Auth_En_False => 0,
        Lw_Ppwr_Falcon_Sctl_Auth_En_True => 1
    );

---------- Record Declaration ----------

    type LW_PPWR_FALCON_SCTL_REGISTER is
    record
        F_Lsmode    : LW_PPWR_FALCON_SCTL_LSMODE_FIELD;
        F_Hsmode    : LW_PPWR_FALCON_SCTL_HSMODE_FIELD;
        F_Lsmode_Level    : LW_PPWR_FALCON_SCTL_LSMODE_LEVEL_FIELD;
        F_Debug_Priv_Level    : LW_PPWR_FALCON_SCTL_DEBUG_PRIV_LEVEL_FIELD;
        F_Reset_Lvlm_En    : LW_PPWR_FALCON_SCTL_RESET_LVLM_EN_FIELD;
        F_Stallreq_Clr_En    : LW_PPWR_FALCON_SCTL_STALLREQ_CLR_EN_FIELD;
        F_Auth_En    : LW_PPWR_FALCON_SCTL_AUTH_EN_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPWR_FALCON_SCTL_REGISTER use
    record
        F_Lsmode at 0 range 0 .. 0;
        F_Hsmode at 0 range 1 .. 1;
        F_Lsmode_Level at 0 range 4 .. 5;
        F_Debug_Priv_Level at 0 range 8 .. 9;
        F_Reset_Lvlm_En at 0 range 12 .. 12;
        F_Stallreq_Clr_En at 0 range 13 .. 13;
        F_Auth_En at 0 range 14 .. 14;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppwr_Falcon_Cpuctl    : constant LW_PPWR_BAR0_ADDR := 16#0010_A100#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPWR_FALCON_CPUCTL_IILWAL_FIELD is
    (
        Lw_Ppwr_Falcon_Cpuctl_Iilwal_False,
        Lw_Ppwr_Falcon_Cpuctl_Iilwal_True
    ) with size => 1;

    for LW_PPWR_FALCON_CPUCTL_IILWAL_FIELD use
    (
        Lw_Ppwr_Falcon_Cpuctl_Iilwal_False => 0,
        Lw_Ppwr_Falcon_Cpuctl_Iilwal_True => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_CPUCTL_STARTCPU_FIELD is
    (
        Lw_Ppwr_Falcon_Cpuctl_Startcpu_False,
        Lw_Ppwr_Falcon_Cpuctl_Startcpu_True
    ) with size => 1;

    for LW_PPWR_FALCON_CPUCTL_STARTCPU_FIELD use
    (
        Lw_Ppwr_Falcon_Cpuctl_Startcpu_False => 0,
        Lw_Ppwr_Falcon_Cpuctl_Startcpu_True => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_CPUCTL_SRESET_FIELD is
    (
        Lw_Ppwr_Falcon_Cpuctl_Sreset_False,
        Lw_Ppwr_Falcon_Cpuctl_Sreset_True
    ) with size => 1;

    for LW_PPWR_FALCON_CPUCTL_SRESET_FIELD use
    (
        Lw_Ppwr_Falcon_Cpuctl_Sreset_False => 0,
        Lw_Ppwr_Falcon_Cpuctl_Sreset_True => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_CPUCTL_HRESET_FIELD is
    (
        Lw_Ppwr_Falcon_Cpuctl_Hreset_False,
        Lw_Ppwr_Falcon_Cpuctl_Hreset_True
    ) with size => 1;

    for LW_PPWR_FALCON_CPUCTL_HRESET_FIELD use
    (
        Lw_Ppwr_Falcon_Cpuctl_Hreset_False => 0,
        Lw_Ppwr_Falcon_Cpuctl_Hreset_True => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_CPUCTL_HALTED_FIELD is
    (
        Lw_Ppwr_Falcon_Cpuctl_Halted_False,
        Lw_Ppwr_Falcon_Cpuctl_Halted_True
    ) with size => 1;

    for LW_PPWR_FALCON_CPUCTL_HALTED_FIELD use
    (
        Lw_Ppwr_Falcon_Cpuctl_Halted_False => 0,
        Lw_Ppwr_Falcon_Cpuctl_Halted_True => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_CPUCTL_STOPPED_FIELD is
    (
        Lw_Ppwr_Falcon_Cpuctl_Stopped_False,
        Lw_Ppwr_Falcon_Cpuctl_Stopped_True
    ) with size => 1;

    for LW_PPWR_FALCON_CPUCTL_STOPPED_FIELD use
    (
        Lw_Ppwr_Falcon_Cpuctl_Stopped_False => 0,
        Lw_Ppwr_Falcon_Cpuctl_Stopped_True => 1
    );

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_CPUCTL_ALIAS_EN_FIELD is
    (
        Lw_Ppwr_Falcon_Cpuctl_Alias_En_False,
        Lw_Ppwr_Falcon_Cpuctl_Alias_En_True
    ) with size => 1;

    for LW_PPWR_FALCON_CPUCTL_ALIAS_EN_FIELD use
    (
        Lw_Ppwr_Falcon_Cpuctl_Alias_En_False => 0,
        Lw_Ppwr_Falcon_Cpuctl_Alias_En_True => 1
    );

    Lw_Ppwr_Falcon_Cpuctl_Alias_En_Init :
        constant LW_PPWR_FALCON_CPUCTL_ALIAS_EN_FIELD
        := Lw_Ppwr_Falcon_Cpuctl_Alias_En_False;

---------- Record Declaration ----------

    type LW_PPWR_FALCON_CPUCTL_REGISTER is
    record
        F_Iilwal    : LW_PPWR_FALCON_CPUCTL_IILWAL_FIELD;
        F_Startcpu    : LW_PPWR_FALCON_CPUCTL_STARTCPU_FIELD;
        F_Sreset    : LW_PPWR_FALCON_CPUCTL_SRESET_FIELD;
        F_Hreset    : LW_PPWR_FALCON_CPUCTL_HRESET_FIELD;
        F_Halted    : LW_PPWR_FALCON_CPUCTL_HALTED_FIELD;
        F_Stopped    : LW_PPWR_FALCON_CPUCTL_STOPPED_FIELD;
        F_Alias_En    : LW_PPWR_FALCON_CPUCTL_ALIAS_EN_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPWR_FALCON_CPUCTL_REGISTER use
    record
        F_Iilwal at 0 range 0 .. 0;
        F_Startcpu at 0 range 1 .. 1;
        F_Sreset at 0 range 2 .. 2;
        F_Hreset at 0 range 3 .. 3;
        F_Halted at 0 range 4 .. 4;
        F_Stopped at 0 range 5 .. 5;
        F_Alias_En at 0 range 6 .. 6;
    end record;

------------------------------------------------------------------------------------------------------
end Dev_Pwr_Pri_Ada;
