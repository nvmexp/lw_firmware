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

--******************************  DEV_FB  ******************************--

package Dev_Fb_Ada
with SPARK_MODE => On
is


---------- Register Range Subtype Declaration ----------
    subtype LW_PFB_BAR0_ADDR is BAR0_ADDR range 16#0010_0000# .. 16#0010_0FFF#;
    subtype LW_PFB_HSMMU_BAR0_ADDR is BAR0_ADDR range 16#001F_AC00# .. 16#001F_AFFF#;
------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pfb_Pri_Mmu_Priv_Level_Mask    : constant LW_PFB_BAR0_ADDR := 16#0010_0CDC#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_REGISTER use
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
end Dev_Fb_Ada;
