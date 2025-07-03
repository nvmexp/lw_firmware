-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;

--******************************  DEV_FBIF_V4  ******************************--

package Dev_Fbif_V4
with SPARK_MODE => On
is

---------- Register Declaration ----------

    Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask    : constant Bar0_Addr := 16#0000_0070#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled,
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Read_Protection_Default_Priv_Level
    ) with size => 4;

    for LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Read_Protection_Only_Level3_Enabled => 8,
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Read_Protection_Default_Priv_Level => 15
    );

    Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Enabled :
        constant LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD
        := Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Read_Protection_Default_Priv_Level;

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled,
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 4;

    for LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Write_Protection_Only_Level3_Enabled => 8,
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 15
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD is
    (
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Source_Read_Control_Lowered,
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Source_Read_Control_Blocked
    ) with size => 1;

    for LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD use
    (
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Source_Read_Control_Lowered => 0,
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Source_Read_Control_Blocked => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD is
    (
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Source_Write_Control_Lowered,
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Source_Write_Control_Blocked
    ) with size => 1;

    for LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD use
    (
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Source_Write_Control_Lowered => 0,
        Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Source_Write_Control_Blocked => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD is new LwU20;
    Lw_Pfalcon_Fbif_Regioncfg_Priv_Level_Mask_Source_Enable_All_Sources_Enabled :
        constant LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD
        := 16#000F_FFFF#;

---------- Record Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Write_Protection    : LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Read_Violation    : LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Violation    : LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
        F_Source_Read_Control    : LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_FIELD;
        F_Source_Write_Control    : LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_FIELD;
        F_Source_Enable    : LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_SOURCE_ENABLE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK_REGISTER use
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

    Lw_Pfalcon_Fbif_Transcfg    : constant Bar0_Addr := 16#0000_0000#;

   LW_PFALCON_FBIF_TRANSCFG_SIZE_1 : constant := 8;
   LW_PFALCON_FBIF_TRANSCFG_MAX_INDEX : constant := LW_PFALCON_FBIF_TRANSCFG_SIZE_1 - 1;
    subtype LW_PFALCON_FBIF_TRANSCFG_INDEX is LwU32 range 0 .. LW_PFALCON_FBIF_TRANSCFG_MAX_INDEX;

    function Lw_Pfalcon_Fbif_Transcfg_Addr( Index : LW_PFALCON_FBIF_TRANSCFG_INDEX )
    return Bar0_Addr is
        (Bar0_Addr(LwU32(Lw_Pfalcon_Fbif_Transcfg) + (LwU32(Index)*4)))
    with Inline_Always,
    Global => null,
    Depends => ( Lw_Pfalcon_Fbif_Transcfg_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_TRANSCFG_TARGET_FIELD is
    (
        Lw_Pfalcon_Fbif_Transcfg_Target_Init,
        Lw_Pfalcon_Fbif_Transcfg_Target_Coherent_Sysmem,
        Lw_Pfalcon_Fbif_Transcfg_Target_Noncoherent_Sysmem
    ) with size => 2;

    for LW_PFALCON_FBIF_TRANSCFG_TARGET_FIELD use
    (
        Lw_Pfalcon_Fbif_Transcfg_Target_Init => 0,
        Lw_Pfalcon_Fbif_Transcfg_Target_Coherent_Sysmem => 1,
        Lw_Pfalcon_Fbif_Transcfg_Target_Noncoherent_Sysmem => 2
    );

    Lw_Pfalcon_Fbif_Transcfg_Target_Local_Fb :
        constant LW_PFALCON_FBIF_TRANSCFG_TARGET_FIELD
        := Lw_Pfalcon_Fbif_Transcfg_Target_Init;

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_TRANSCFG_MEM_TYPE_FIELD is
    (
        Lw_Pfalcon_Fbif_Transcfg_Mem_Type_Init,
        Lw_Pfalcon_Fbif_Transcfg_Mem_Type_Physical
    ) with size => 1;

    for LW_PFALCON_FBIF_TRANSCFG_MEM_TYPE_FIELD use
    (
        Lw_Pfalcon_Fbif_Transcfg_Mem_Type_Init => 0,
        Lw_Pfalcon_Fbif_Transcfg_Mem_Type_Physical => 1
    );

    Lw_Pfalcon_Fbif_Transcfg_Mem_Type_Virtual :
        constant LW_PFALCON_FBIF_TRANSCFG_MEM_TYPE_FIELD
        := Lw_Pfalcon_Fbif_Transcfg_Mem_Type_Init;

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_TRANSCFG_L2C_WR_FIELD is
    (
        Lw_Pfalcon_Fbif_Transcfg_L2C_Wr_L2_Evict_First,
        Lw_Pfalcon_Fbif_Transcfg_L2C_Wr_Init,
        Lw_Pfalcon_Fbif_Transcfg_L2C_Wr_L2_Evict_Last
    ) with size => 2;

    for LW_PFALCON_FBIF_TRANSCFG_L2C_WR_FIELD use
    (
        Lw_Pfalcon_Fbif_Transcfg_L2C_Wr_L2_Evict_First => 0,
        Lw_Pfalcon_Fbif_Transcfg_L2C_Wr_Init => 1,
        Lw_Pfalcon_Fbif_Transcfg_L2C_Wr_L2_Evict_Last => 2
    );

    Lw_Pfalcon_Fbif_Transcfg_L2C_Wr_L2_Evict_Normal :
        constant LW_PFALCON_FBIF_TRANSCFG_L2C_WR_FIELD
        := Lw_Pfalcon_Fbif_Transcfg_L2C_Wr_Init;

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_TRANSCFG_L2C_RD_FIELD is
    (
        Lw_Pfalcon_Fbif_Transcfg_L2C_Rd_L2_Evict_First,
        Lw_Pfalcon_Fbif_Transcfg_L2C_Rd_Init,
        Lw_Pfalcon_Fbif_Transcfg_L2C_Rd_L2_Evict_Last
    ) with size => 2;

    for LW_PFALCON_FBIF_TRANSCFG_L2C_RD_FIELD use
    (
        Lw_Pfalcon_Fbif_Transcfg_L2C_Rd_L2_Evict_First => 0,
        Lw_Pfalcon_Fbif_Transcfg_L2C_Rd_Init => 1,
        Lw_Pfalcon_Fbif_Transcfg_L2C_Rd_L2_Evict_Last => 2
    );

    Lw_Pfalcon_Fbif_Transcfg_L2C_Rd_L2_Evict_Normal :
        constant LW_PFALCON_FBIF_TRANSCFG_L2C_RD_FIELD
        := Lw_Pfalcon_Fbif_Transcfg_L2C_Rd_Init;

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_TRANSCFG_WACHK0_FIELD is
    (
        Lw_Pfalcon_Fbif_Transcfg_Wachk0_Init,
        Lw_Pfalcon_Fbif_Transcfg_Wachk0_Enable
    ) with size => 1;

    for LW_PFALCON_FBIF_TRANSCFG_WACHK0_FIELD use
    (
        Lw_Pfalcon_Fbif_Transcfg_Wachk0_Init => 0,
        Lw_Pfalcon_Fbif_Transcfg_Wachk0_Enable => 1
    );

    Lw_Pfalcon_Fbif_Transcfg_Wachk0_Disable :
        constant LW_PFALCON_FBIF_TRANSCFG_WACHK0_FIELD
        := Lw_Pfalcon_Fbif_Transcfg_Wachk0_Init;

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_TRANSCFG_WACHK1_FIELD is
    (
        Lw_Pfalcon_Fbif_Transcfg_Wachk1_Init,
        Lw_Pfalcon_Fbif_Transcfg_Wachk1_Enable
    ) with size => 1;

    for LW_PFALCON_FBIF_TRANSCFG_WACHK1_FIELD use
    (
        Lw_Pfalcon_Fbif_Transcfg_Wachk1_Init => 0,
        Lw_Pfalcon_Fbif_Transcfg_Wachk1_Enable => 1
    );

    Lw_Pfalcon_Fbif_Transcfg_Wachk1_Disable :
        constant LW_PFALCON_FBIF_TRANSCFG_WACHK1_FIELD
        := Lw_Pfalcon_Fbif_Transcfg_Wachk1_Init;

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_TRANSCFG_RACHK0_FIELD is
    (
        Lw_Pfalcon_Fbif_Transcfg_Rachk0_Init,
        Lw_Pfalcon_Fbif_Transcfg_Rachk0_Enable
    ) with size => 1;

    for LW_PFALCON_FBIF_TRANSCFG_RACHK0_FIELD use
    (
        Lw_Pfalcon_Fbif_Transcfg_Rachk0_Init => 0,
        Lw_Pfalcon_Fbif_Transcfg_Rachk0_Enable => 1
    );

    Lw_Pfalcon_Fbif_Transcfg_Rachk0_Disable :
        constant LW_PFALCON_FBIF_TRANSCFG_RACHK0_FIELD
        := Lw_Pfalcon_Fbif_Transcfg_Rachk0_Init;

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_TRANSCFG_RACHK1_FIELD is
    (
        Lw_Pfalcon_Fbif_Transcfg_Rachk1_Init,
        Lw_Pfalcon_Fbif_Transcfg_Rachk1_Enable
    ) with size => 1;

    for LW_PFALCON_FBIF_TRANSCFG_RACHK1_FIELD use
    (
        Lw_Pfalcon_Fbif_Transcfg_Rachk1_Init => 0,
        Lw_Pfalcon_Fbif_Transcfg_Rachk1_Enable => 1
    );

    Lw_Pfalcon_Fbif_Transcfg_Rachk1_Disable :
        constant LW_PFALCON_FBIF_TRANSCFG_RACHK1_FIELD
        := Lw_Pfalcon_Fbif_Transcfg_Rachk1_Init;

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_TRANSCFG_ENGINE_ID_FLAG_FIELD is
    (
        Lw_Pfalcon_Fbif_Transcfg_Engine_Id_Flag_Bar2_Fn0,
        Lw_Pfalcon_Fbif_Transcfg_Engine_Id_Flag_Own
    ) with size => 1;

    for LW_PFALCON_FBIF_TRANSCFG_ENGINE_ID_FLAG_FIELD use
    (
        Lw_Pfalcon_Fbif_Transcfg_Engine_Id_Flag_Bar2_Fn0 => 0,
        Lw_Pfalcon_Fbif_Transcfg_Engine_Id_Flag_Own => 1
    );

---------- Record Declaration ----------

    type LW_PFALCON_FBIF_TRANSCFG_REGISTER is
    record
        F_Target    : LW_PFALCON_FBIF_TRANSCFG_TARGET_FIELD;
        F_Mem_Type    : LW_PFALCON_FBIF_TRANSCFG_MEM_TYPE_FIELD;
        F_L2C_Wr    : LW_PFALCON_FBIF_TRANSCFG_L2C_WR_FIELD;
        F_L2C_Rd    : LW_PFALCON_FBIF_TRANSCFG_L2C_RD_FIELD;
        F_Wachk0    : LW_PFALCON_FBIF_TRANSCFG_WACHK0_FIELD;
        F_Wachk1    : LW_PFALCON_FBIF_TRANSCFG_WACHK1_FIELD;
        F_Rachk0    : LW_PFALCON_FBIF_TRANSCFG_RACHK0_FIELD;
        F_Rachk1    : LW_PFALCON_FBIF_TRANSCFG_RACHK1_FIELD;
        F_Engine_Id_Flag    : LW_PFALCON_FBIF_TRANSCFG_ENGINE_ID_FLAG_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FBIF_TRANSCFG_REGISTER use
    record
        F_Target at 0 range 0 .. 1;
        F_Mem_Type at 0 range 2 .. 2;
        F_L2C_Wr at 0 range 4 .. 5;
        F_L2C_Rd at 0 range 8 .. 9;
        F_Wachk0 at 0 range 12 .. 12;
        F_Wachk1 at 0 range 13 .. 13;
        F_Rachk0 at 0 range 14 .. 14;
        F_Rachk1 at 0 range 15 .. 15;
        F_Engine_Id_Flag at 0 range 16 .. 16;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pfalcon_Fbif_Instblk    : constant Bar0_Addr := 16#0000_0020#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_INSTBLK_PTR_FIELD is new LwU28;
---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_INSTBLK_TARGET_FIELD is
    (
        Lw_Pfalcon_Fbif_Instblk_Target_Local_Fb,
        Lw_Pfalcon_Fbif_Instblk_Target_Coherent_Sysmem,
        Lw_Pfalcon_Fbif_Instblk_Target_Noncoherent_Sysmem
    ) with size => 2;

    for LW_PFALCON_FBIF_INSTBLK_TARGET_FIELD use
    (
        Lw_Pfalcon_Fbif_Instblk_Target_Local_Fb => 0,
        Lw_Pfalcon_Fbif_Instblk_Target_Coherent_Sysmem => 1,
        Lw_Pfalcon_Fbif_Instblk_Target_Noncoherent_Sysmem => 2
    );

---------- Record Declaration ----------

    type LW_PFALCON_FBIF_INSTBLK_REGISTER is
    record
        F_Ptr    : LW_PFALCON_FBIF_INSTBLK_PTR_FIELD;
        F_Target    : LW_PFALCON_FBIF_INSTBLK_TARGET_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FBIF_INSTBLK_REGISTER use
    record
        F_Ptr at 0 range 0 .. 27;
        F_Target at 0 range 28 .. 29;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pfalcon_Fbif_Bind_Status    : constant Bar0_Addr := 16#0000_0028#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_BIND_STATUS_STATUS_FIELD is
    (
        Lw_Pfalcon_Fbif_Bind_Status_Status_Not_Bind,
        Lw_Pfalcon_Fbif_Bind_Status_Status_Bound
    ) with size => 2;

    for LW_PFALCON_FBIF_BIND_STATUS_STATUS_FIELD use
    (
        Lw_Pfalcon_Fbif_Bind_Status_Status_Not_Bind => 0,
        Lw_Pfalcon_Fbif_Bind_Status_Status_Bound => 2
    );

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_BIND_STATUS_GFID_FIELD is new LwU6;
    Lw_Pfalcon_Fbif_Bind_Status_Gfid_Init :
        constant LW_PFALCON_FBIF_BIND_STATUS_GFID_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_BIND_STATUS_ENGINE_ID_FIELD is new LwU8;
---------- Record Declaration ----------

    type LW_PFALCON_FBIF_BIND_STATUS_REGISTER is
    record
        F_Status    : LW_PFALCON_FBIF_BIND_STATUS_STATUS_FIELD;
        F_Gfid    : LW_PFALCON_FBIF_BIND_STATUS_GFID_FIELD;
        F_Engine_Id    : LW_PFALCON_FBIF_BIND_STATUS_ENGINE_ID_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FBIF_BIND_STATUS_REGISTER use
    record
        F_Status at 0 range 0 .. 1;
        F_Gfid at 0 range 4 .. 9;
        F_Engine_Id at 0 range 16 .. 23;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pfalcon_Fbif_Ctl    : constant Bar0_Addr := 16#0000_0024#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_CTL_FLUSH_FIELD is
    (
        Lw_Pfalcon_Fbif_Ctl_Flush_Clear,
        Lw_Pfalcon_Fbif_Ctl_Flush_Set
    ) with size => 1;

    for LW_PFALCON_FBIF_CTL_FLUSH_FIELD use
    (
        Lw_Pfalcon_Fbif_Ctl_Flush_Clear => 0,
        Lw_Pfalcon_Fbif_Ctl_Flush_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_CTL_ILWAL_CONTEXT_FIELD is
    (
        Lw_Pfalcon_Fbif_Ctl_Ilwal_Context_Clear,
        Lw_Pfalcon_Fbif_Ctl_Ilwal_Context_Set
    ) with size => 1;

    for LW_PFALCON_FBIF_CTL_ILWAL_CONTEXT_FIELD use
    (
        Lw_Pfalcon_Fbif_Ctl_Ilwal_Context_Clear => 0,
        Lw_Pfalcon_Fbif_Ctl_Ilwal_Context_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_CTL_CLR_BWCOUNT_FIELD is
    (
        Lw_Pfalcon_Fbif_Ctl_Clr_Bwcount_Clear,
        Lw_Pfalcon_Fbif_Ctl_Clr_Bwcount_Set
    ) with size => 1;

    for LW_PFALCON_FBIF_CTL_CLR_BWCOUNT_FIELD use
    (
        Lw_Pfalcon_Fbif_Ctl_Clr_Bwcount_Clear => 0,
        Lw_Pfalcon_Fbif_Ctl_Clr_Bwcount_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_CTL_ENABLE_FIELD is
    (
        Lw_Pfalcon_Fbif_Ctl_Enable_False,
        Lw_Pfalcon_Fbif_Ctl_Enable_Init
    ) with size => 1;

    for LW_PFALCON_FBIF_CTL_ENABLE_FIELD use
    (
        Lw_Pfalcon_Fbif_Ctl_Enable_False => 0,
        Lw_Pfalcon_Fbif_Ctl_Enable_Init => 1
    );

    Lw_Pfalcon_Fbif_Ctl_Enable_True :
        constant LW_PFALCON_FBIF_CTL_ENABLE_FIELD
        := Lw_Pfalcon_Fbif_Ctl_Enable_Init;

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_CTL_CLR_IDLEWDERR_FIELD is
    (
        Lw_Pfalcon_Fbif_Ctl_Clr_Idlewderr_Clear,
        Lw_Pfalcon_Fbif_Ctl_Clr_Idlewderr_Set
    ) with size => 1;

    for LW_PFALCON_FBIF_CTL_CLR_IDLEWDERR_FIELD use
    (
        Lw_Pfalcon_Fbif_Ctl_Clr_Idlewderr_Clear => 0,
        Lw_Pfalcon_Fbif_Ctl_Clr_Idlewderr_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_CTL_RESET_FIELD is
    (
        Lw_Pfalcon_Fbif_Ctl_Reset_Clear,
        Lw_Pfalcon_Fbif_Ctl_Reset_Set
    ) with size => 1;

    for LW_PFALCON_FBIF_CTL_RESET_FIELD use
    (
        Lw_Pfalcon_Fbif_Ctl_Reset_Clear => 0,
        Lw_Pfalcon_Fbif_Ctl_Reset_Set => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_CTL_ALLOW_PHYS_NO_CTX_FIELD is
    (
        Lw_Pfalcon_Fbif_Ctl_Allow_Phys_No_Ctx_Init,
        Lw_Pfalcon_Fbif_Ctl_Allow_Phys_No_Ctx_Allow
    ) with size => 1;

    for LW_PFALCON_FBIF_CTL_ALLOW_PHYS_NO_CTX_FIELD use
    (
        Lw_Pfalcon_Fbif_Ctl_Allow_Phys_No_Ctx_Init => 0,
        Lw_Pfalcon_Fbif_Ctl_Allow_Phys_No_Ctx_Allow => 1
    );

    Lw_Pfalcon_Fbif_Ctl_Allow_Phys_No_Ctx_Disallow :
        constant LW_PFALCON_FBIF_CTL_ALLOW_PHYS_NO_CTX_FIELD
        := Lw_Pfalcon_Fbif_Ctl_Allow_Phys_No_Ctx_Init;

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_CTL_IDLE_FIELD is
    (
        Lw_Pfalcon_Fbif_Ctl_Idle_False,
        Lw_Pfalcon_Fbif_Ctl_Idle_True
    ) with size => 1;

    for LW_PFALCON_FBIF_CTL_IDLE_FIELD use
    (
        Lw_Pfalcon_Fbif_Ctl_Idle_False => 0,
        Lw_Pfalcon_Fbif_Ctl_Idle_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_CTL_IDLEWDERR_FIELD is
    (
        Lw_Pfalcon_Fbif_Ctl_Idlewderr_False,
        Lw_Pfalcon_Fbif_Ctl_Idlewderr_True
    ) with size => 1;

    for LW_PFALCON_FBIF_CTL_IDLEWDERR_FIELD use
    (
        Lw_Pfalcon_Fbif_Ctl_Idlewderr_False => 0,
        Lw_Pfalcon_Fbif_Ctl_Idlewderr_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_CTL_SRTOUT_FIELD is
    (
        Lw_Pfalcon_Fbif_Ctl_Srtout_False,
        Lw_Pfalcon_Fbif_Ctl_Srtout_True
    ) with size => 1;

    for LW_PFALCON_FBIF_CTL_SRTOUT_FIELD use
    (
        Lw_Pfalcon_Fbif_Ctl_Srtout_False => 0,
        Lw_Pfalcon_Fbif_Ctl_Srtout_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_CTL_CLR_SRTOUT_FIELD is
    (
        Lw_Pfalcon_Fbif_Ctl_Clr_Srtout_Clear,
        Lw_Pfalcon_Fbif_Ctl_Clr_Srtout_Set
    ) with size => 1;

    for LW_PFALCON_FBIF_CTL_CLR_SRTOUT_FIELD use
    (
        Lw_Pfalcon_Fbif_Ctl_Clr_Srtout_Clear => 0,
        Lw_Pfalcon_Fbif_Ctl_Clr_Srtout_Set => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_CTL_SRTOVAL_FIELD is new LwU4;
    Lw_Pfalcon_Fbif_Ctl_Srtoval_Init :
        constant LW_PFALCON_FBIF_CTL_SRTOVAL_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PFALCON_FBIF_CTL_REGISTER is
    record
        F_Flush    : LW_PFALCON_FBIF_CTL_FLUSH_FIELD;
        F_Ilwal_Context    : LW_PFALCON_FBIF_CTL_ILWAL_CONTEXT_FIELD;
        F_Clr_Bwcount    : LW_PFALCON_FBIF_CTL_CLR_BWCOUNT_FIELD;
        F_Enable    : LW_PFALCON_FBIF_CTL_ENABLE_FIELD;
        F_Clr_Idlewderr    : LW_PFALCON_FBIF_CTL_CLR_IDLEWDERR_FIELD;
        F_Reset    : LW_PFALCON_FBIF_CTL_RESET_FIELD;
        F_Allow_Phys_No_Ctx    : LW_PFALCON_FBIF_CTL_ALLOW_PHYS_NO_CTX_FIELD;
        F_Idle    : LW_PFALCON_FBIF_CTL_IDLE_FIELD;
        F_Idlewderr    : LW_PFALCON_FBIF_CTL_IDLEWDERR_FIELD;
        F_Srtout    : LW_PFALCON_FBIF_CTL_SRTOUT_FIELD;
        F_Clr_Srtout    : LW_PFALCON_FBIF_CTL_CLR_SRTOUT_FIELD;
        F_Srtoval    : LW_PFALCON_FBIF_CTL_SRTOVAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FBIF_CTL_REGISTER use
    record
        F_Flush at 0 range 0 .. 0;
        F_Ilwal_Context at 0 range 2 .. 2;
        F_Clr_Bwcount at 0 range 3 .. 3;
        F_Enable at 0 range 4 .. 4;
        F_Clr_Idlewderr at 0 range 5 .. 5;
        F_Reset at 0 range 6 .. 6;
        F_Allow_Phys_No_Ctx at 0 range 7 .. 7;
        F_Idle at 0 range 8 .. 8;
        F_Idlewderr at 0 range 9 .. 9;
        F_Srtout at 0 range 10 .. 10;
        F_Clr_Srtout at 0 range 11 .. 11;
        F_Srtoval at 0 range 16 .. 19;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pfalcon_Fbif_Throttle    : constant Bar0_Addr := 16#0000_002C#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_THROTTLE_BUCKET_SIZE_FIELD is new LwU12;
    Lw_Pfalcon_Fbif_Throttle_Bucket_Size_Init :
        constant LW_PFALCON_FBIF_THROTTLE_BUCKET_SIZE_FIELD
        := 16#0000_0064#;

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_THROTTLE_LEAK_COUNT_FIELD is new LwU12;
    Lw_Pfalcon_Fbif_Throttle_Leak_Count_Disable :
        constant LW_PFALCON_FBIF_THROTTLE_LEAK_COUNT_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_THROTTLE_LEAK_SIZE_FIELD is
    (
        Lw_Pfalcon_Fbif_Throttle_Leak_Size_16B,
        Lw_Pfalcon_Fbif_Throttle_Leak_Size_32B,
        Lw_Pfalcon_Fbif_Throttle_Leak_Size_64B,
        Lw_Pfalcon_Fbif_Throttle_Leak_Size_128B
    ) with size => 2;

    for LW_PFALCON_FBIF_THROTTLE_LEAK_SIZE_FIELD use
    (
        Lw_Pfalcon_Fbif_Throttle_Leak_Size_16B => 0,
        Lw_Pfalcon_Fbif_Throttle_Leak_Size_32B => 1,
        Lw_Pfalcon_Fbif_Throttle_Leak_Size_64B => 2,
        Lw_Pfalcon_Fbif_Throttle_Leak_Size_128B => 3
    );

---------- Record Declaration ----------

    type LW_PFALCON_FBIF_THROTTLE_REGISTER is
    record
        F_Bucket_Size    : LW_PFALCON_FBIF_THROTTLE_BUCKET_SIZE_FIELD;
        F_Leak_Count    : LW_PFALCON_FBIF_THROTTLE_LEAK_COUNT_FIELD;
        F_Leak_Size    : LW_PFALCON_FBIF_THROTTLE_LEAK_SIZE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FBIF_THROTTLE_REGISTER use
    record
        F_Bucket_Size at 0 range 0 .. 11;
        F_Leak_Count at 0 range 16 .. 27;
        F_Leak_Size at 0 range 30 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pfalcon_Fbif_Regioncfg    : constant Bar0_Addr := 16#0000_006C#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_T0_FIELD is new LwU4;
    Lw_Pfalcon_Fbif_Regioncfg_T0_Init :
        constant LW_PFALCON_FBIF_REGIONCFG_T0_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_T1_FIELD is new LwU4;
    Lw_Pfalcon_Fbif_Regioncfg_T1_Init :
        constant LW_PFALCON_FBIF_REGIONCFG_T1_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_T2_FIELD is new LwU4;
    Lw_Pfalcon_Fbif_Regioncfg_T2_Init :
        constant LW_PFALCON_FBIF_REGIONCFG_T2_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_T3_FIELD is new LwU4;
    Lw_Pfalcon_Fbif_Regioncfg_T3_Init :
        constant LW_PFALCON_FBIF_REGIONCFG_T3_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_T4_FIELD is new LwU4;
    Lw_Pfalcon_Fbif_Regioncfg_T4_Init :
        constant LW_PFALCON_FBIF_REGIONCFG_T4_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_T5_FIELD is new LwU4;
    Lw_Pfalcon_Fbif_Regioncfg_T5_Init :
        constant LW_PFALCON_FBIF_REGIONCFG_T5_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_T6_FIELD is new LwU4;
    Lw_Pfalcon_Fbif_Regioncfg_T6_Init :
        constant LW_PFALCON_FBIF_REGIONCFG_T6_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_T7_FIELD is new LwU4;
    Lw_Pfalcon_Fbif_Regioncfg_T7_Init :
        constant LW_PFALCON_FBIF_REGIONCFG_T7_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PFALCON_FBIF_REGIONCFG_REGISTER is
    record
        F_T0    : LW_PFALCON_FBIF_REGIONCFG_T0_FIELD;
        F_T1    : LW_PFALCON_FBIF_REGIONCFG_T1_FIELD;
        F_T2    : LW_PFALCON_FBIF_REGIONCFG_T2_FIELD;
        F_T3    : LW_PFALCON_FBIF_REGIONCFG_T3_FIELD;
        F_T4    : LW_PFALCON_FBIF_REGIONCFG_T4_FIELD;
        F_T5    : LW_PFALCON_FBIF_REGIONCFG_T5_FIELD;
        F_T6    : LW_PFALCON_FBIF_REGIONCFG_T6_FIELD;
        F_T7    : LW_PFALCON_FBIF_REGIONCFG_T7_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FBIF_REGIONCFG_REGISTER use
    record
        F_T0 at 0 range 0 .. 3;
        F_T1 at 0 range 4 .. 7;
        F_T2 at 0 range 8 .. 11;
        F_T3 at 0 range 12 .. 15;
        F_T4 at 0 range 16 .. 19;
        F_T5 at 0 range 20 .. 23;
        F_T6 at 0 range 24 .. 27;
        F_T7 at 0 range 28 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pfalcon_Fbif_Cg1    : constant Bar0_Addr := 16#0000_0074#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_CG1_SLCG_MSD0_FIELD is
    (
        Lw_Pfalcon_Fbif_Cg1_Slcg_Msd0_Enabled,
        Lw_Pfalcon_Fbif_Cg1_Slcg_Msd0_Disabled
    ) with size => 1;

    for LW_PFALCON_FBIF_CG1_SLCG_MSD0_FIELD use
    (
        Lw_Pfalcon_Fbif_Cg1_Slcg_Msd0_Enabled => 0,
        Lw_Pfalcon_Fbif_Cg1_Slcg_Msd0_Disabled => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_CG1_SLCG_MSD1_FIELD is
    (
        Lw_Pfalcon_Fbif_Cg1_Slcg_Msd1_Enabled,
        Lw_Pfalcon_Fbif_Cg1_Slcg_Msd1_Disabled
    ) with size => 1;

    for LW_PFALCON_FBIF_CG1_SLCG_MSD1_FIELD use
    (
        Lw_Pfalcon_Fbif_Cg1_Slcg_Msd1_Enabled => 0,
        Lw_Pfalcon_Fbif_Cg1_Slcg_Msd1_Disabled => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_CG1_SLCG_FB0_FIELD is
    (
        Lw_Pfalcon_Fbif_Cg1_Slcg_Fb0_Enabled,
        Lw_Pfalcon_Fbif_Cg1_Slcg_Fb0_Disabled
    ) with size => 1;

    for LW_PFALCON_FBIF_CG1_SLCG_FB0_FIELD use
    (
        Lw_Pfalcon_Fbif_Cg1_Slcg_Fb0_Enabled => 0,
        Lw_Pfalcon_Fbif_Cg1_Slcg_Fb0_Disabled => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_CG1_SLCG_FB1_FIELD is
    (
        Lw_Pfalcon_Fbif_Cg1_Slcg_Fb1_Enabled,
        Lw_Pfalcon_Fbif_Cg1_Slcg_Fb1_Disabled
    ) with size => 1;

    for LW_PFALCON_FBIF_CG1_SLCG_FB1_FIELD use
    (
        Lw_Pfalcon_Fbif_Cg1_Slcg_Fb1_Enabled => 0,
        Lw_Pfalcon_Fbif_Cg1_Slcg_Fb1_Disabled => 1
    );

---------- Record Declaration ----------

    type LW_PFALCON_FBIF_CG1_REGISTER is
    record
        F_Slcg_Msd0    : LW_PFALCON_FBIF_CG1_SLCG_MSD0_FIELD;
        F_Slcg_Msd1    : LW_PFALCON_FBIF_CG1_SLCG_MSD1_FIELD;
        F_Slcg_Fb0    : LW_PFALCON_FBIF_CG1_SLCG_FB0_FIELD;
        F_Slcg_Fb1    : LW_PFALCON_FBIF_CG1_SLCG_FB1_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FBIF_CG1_REGISTER use
    record
        F_Slcg_Msd0 at 0 range 0 .. 0;
        F_Slcg_Msd1 at 0 range 1 .. 1;
        F_Slcg_Fb0 at 0 range 2 .. 2;
        F_Slcg_Fb1 at 0 range 3 .. 3;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pfalcon_Fbif_Achk_Blk    : constant Bar0_Addr := 16#0000_0030#;

   LW_PFALCON_FBIF_ACHK_BLK_SIZE_1 : constant := 2;
   LW_PFALCON_FBIF_ACHK_BLK_MAX_INDEX : constant := LW_PFALCON_FBIF_ACHK_BLK_SIZE_1 - 1;
   subtype LW_PFALCON_FBIF_ACHK_BLK_INDEX is LwU32 range 0 .. LW_PFALCON_FBIF_ACHK_BLK_MAX_INDEX;

    function Lw_Pfalcon_Fbif_Achk_Blk_Addr( Index : LW_PFALCON_FBIF_ACHK_BLK_INDEX )
    return Bar0_Addr is
        (Bar0_Addr(LwU32(Lw_Pfalcon_Fbif_Achk_Blk) + (LwU32(Index)*8)))
    with Inline_Always,
    Global => null,
    Depends => ( Lw_Pfalcon_Fbif_Achk_Blk_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_ACHK_BLK_ADDR_FIELD is new LwU32;
---------- Record Declaration ----------

    type LW_PFALCON_FBIF_ACHK_BLK_REGISTER is
    record
        F_Addr    : LW_PFALCON_FBIF_ACHK_BLK_ADDR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FBIF_ACHK_BLK_REGISTER use
    record
        F_Addr at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pfalcon_Fbif_Achk_Ctl    : constant Bar0_Addr := 16#0000_0034#;

   LW_PFALCON_FBIF_ACHK_CTL_SIZE_1 : constant := 2;
   LW_PFALCON_FBIF_ACHK_CTL_MAX_INDEX : constant := LW_PFALCON_FBIF_ACHK_CTL_SIZE_1 - 1;
    subtype LW_PFALCON_FBIF_ACHK_CTL_INDEX is LwU32 range 0 .. LW_PFALCON_FBIF_ACHK_CTL_MAX_INDEX;

    function Lw_Pfalcon_Fbif_Achk_Ctl_Addr( Index : LW_PFALCON_FBIF_ACHK_CTL_INDEX )
    return Bar0_Addr is
        (Bar0_Addr(LwU32(Lw_Pfalcon_Fbif_Achk_Ctl) + (LwU32(Index)*8)))
    with Inline_Always,
    Global => null,
    Depends => ( Lw_Pfalcon_Fbif_Achk_Ctl_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_ACHK_CTL_SIZE_FIELD is new LwU5;
---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_ACHK_CTL_TYPE_FIELD is
    (
        Lw_Pfalcon_Fbif_Achk_Ctl_Type_Inside,
        Lw_Pfalcon_Fbif_Achk_Ctl_Type_Outside
    ) with size => 1;

    for LW_PFALCON_FBIF_ACHK_CTL_TYPE_FIELD use
    (
        Lw_Pfalcon_Fbif_Achk_Ctl_Type_Inside => 0,
        Lw_Pfalcon_Fbif_Achk_Ctl_Type_Outside => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_ACHK_CTL_STATE_FIELD is
    (
        Lw_Pfalcon_Fbif_Achk_Ctl_State_Disable,
        Lw_Pfalcon_Fbif_Achk_Ctl_State_Enable
    ) with size => 1;

    for LW_PFALCON_FBIF_ACHK_CTL_STATE_FIELD use
    (
        Lw_Pfalcon_Fbif_Achk_Ctl_State_Disable => 0,
        Lw_Pfalcon_Fbif_Achk_Ctl_State_Enable => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_ACHK_CTL_COUNT_FIELD is new LwU16;
---------- Record Declaration ----------

    type LW_PFALCON_FBIF_ACHK_CTL_REGISTER is
    record
        F_Size    : LW_PFALCON_FBIF_ACHK_CTL_SIZE_FIELD;
        F_Type    : LW_PFALCON_FBIF_ACHK_CTL_TYPE_FIELD;
        F_State    : LW_PFALCON_FBIF_ACHK_CTL_STATE_FIELD;
        F_Count    : LW_PFALCON_FBIF_ACHK_CTL_COUNT_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FBIF_ACHK_CTL_REGISTER use
    record
        F_Size at 0 range 0 .. 4;
        F_Type at 0 range 6 .. 6;
        F_State at 0 range 7 .. 7;
        F_Count at 0 range 16 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pfalcon_Fbif_Bw_Alloc    : constant Bar0_Addr := 16#0000_004C#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_BW_ALLOC_INT_RD_FIELD is new LwU8;
---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_BW_ALLOC_INT_WR_FIELD is new LwU8;
---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_BW_ALLOC_EXT_RD_FIELD is new LwU8;
---------- Record Field Type Declaration ----------

    type LW_PFALCON_FBIF_BW_ALLOC_REQ_T_FIELD is new LwU8;
---------- Record Declaration ----------

    type LW_PFALCON_FBIF_BW_ALLOC_REGISTER is
    record
        F_Int_Rd    : LW_PFALCON_FBIF_BW_ALLOC_INT_RD_FIELD;
        F_Int_Wr    : LW_PFALCON_FBIF_BW_ALLOC_INT_WR_FIELD;
        F_Ext_Rd    : LW_PFALCON_FBIF_BW_ALLOC_EXT_RD_FIELD;
        F_Req_T    : LW_PFALCON_FBIF_BW_ALLOC_REQ_T_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FBIF_BW_ALLOC_REGISTER use
    record
        F_Int_Rd at 0 range 0 .. 7;
        F_Int_Wr at 0 range 8 .. 15;
        F_Ext_Rd at 0 range 16 .. 23;
        F_Req_T at 0 range 24 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
end Dev_Fbif_V4;
