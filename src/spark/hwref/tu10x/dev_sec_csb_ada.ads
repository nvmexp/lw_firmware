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

--******************************  DEV_SEC_CSB  ******************************--

package Dev_Sec_Csb_Ada
with SPARK_MODE => On
is

---------- Register Declaration ----------

    Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask    : constant CSB_ADDR := 16#0000_A400#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_CSEC_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_CSEC_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_CSEC_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_CSEC_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Csec_Falcon_Irqtmr_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Record Declaration ----------

    type LW_CSEC_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_CSEC_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Read_Violation    : LW_CSEC_FALCON_IRQTMR_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Protection    : LW_CSEC_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Write_Violation    : LW_CSEC_FALCON_IRQTMR_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 2;
        F_Read_Violation at 0 range 3 .. 3;
        F_Write_Protection at 0 range 4 .. 6;
        F_Write_Violation at 0 range 7 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Reset_Priv_Level_Mask    : constant CSB_ADDR := 16#0000_F100#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD is
    (
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Read_Protection_All_Levels_Disabled,
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Read_Protection_Level2_Enabled,
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Read_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD use
    (
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Read_Protection_All_Levels_Disabled => 0,
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Read_Protection_Level2_Enabled => 4,
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Read_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD is
    (
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Read_Violation_Soldier_On,
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Read_Violation_Report_Error
    ) with size => 1;

    for LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD use
    (
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Read_Violation_Soldier_On => 0,
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Read_Violation_Report_Error => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD is
    (
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Write_Protection_All_Levels_Disabled,
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Write_Protection_Level2_Enabled,
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Write_Protection_All_Levels_Enabled
    ) with size => 3;

    for LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD use
    (
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Write_Protection_All_Levels_Disabled => 0,
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Write_Protection_Level2_Enabled => 4,
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Write_Protection_All_Levels_Enabled => 7
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD is
    (
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Write_Violation_Soldier_On,
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Write_Violation_Report_Error
    ) with size => 1;

    for LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD use
    (
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Write_Violation_Soldier_On => 0,
        Lw_Csec_Falcon_Reset_Priv_Level_Mask_Write_Violation_Report_Error => 1
    );

---------- Record Declaration ----------

    type LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_REGISTER is
    record
        F_Read_Protection    : LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_READ_PROTECTION_FIELD;
        F_Read_Violation    : LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_READ_VIOLATION_FIELD;
        F_Write_Protection    : LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_WRITE_PROTECTION_FIELD;
        F_Write_Violation    : LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_WRITE_VIOLATION_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_REGISTER use
    record
        F_Read_Protection at 0 range 0 .. 2;
        F_Read_Violation at 0 range 3 .. 3;
        F_Write_Protection at 0 range 4 .. 6;
        F_Write_Violation at 0 range 7 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Irqmset    : constant CSB_ADDR := 16#0000_0400#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_GPTMR_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Gptmr_Set :
        constant LW_CSEC_FALCON_IRQMSET_GPTMR_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_WDTMR_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Wdtmr_Set :
        constant LW_CSEC_FALCON_IRQMSET_WDTMR_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_MTHD_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Mthd_Set :
        constant LW_CSEC_FALCON_IRQMSET_MTHD_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_CTXSW_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Ctxsw_Set :
        constant LW_CSEC_FALCON_IRQMSET_CTXSW_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_HALT_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Halt_Set :
        constant LW_CSEC_FALCON_IRQMSET_HALT_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_EXTERR_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Exterr_Set :
        constant LW_CSEC_FALCON_IRQMSET_EXTERR_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_SWGEN0_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Swgen0_Set :
        constant LW_CSEC_FALCON_IRQMSET_SWGEN0_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_SWGEN1_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Swgen1_Set :
        constant LW_CSEC_FALCON_IRQMSET_SWGEN1_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ1_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Ext_Extirq1_Set :
        constant LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ1_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ2_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Ext_Extirq2_Set :
        constant LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ2_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ3_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Ext_Extirq3_Set :
        constant LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ3_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ4_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Ext_Extirq4_Set :
        constant LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ4_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ5_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Ext_Extirq5_Set :
        constant LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ5_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ6_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Ext_Extirq6_Set :
        constant LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ6_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ7_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Ext_Extirq7_Set :
        constant LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ7_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ8_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Ext_Extirq8_Set :
        constant LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ8_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_DMA_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Dma_Set :
        constant LW_CSEC_FALCON_IRQMSET_DMA_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_SHA_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Sha_Set :
        constant LW_CSEC_FALCON_IRQMSET_SHA_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_MEMERR_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Memerr_Set :
        constant LW_CSEC_FALCON_IRQMSET_MEMERR_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_FALCON_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Falcon_Set :
        constant LW_CSEC_FALCON_IRQMSET_FALCON_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_RISCV_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Riscv_Set :
        constant LW_CSEC_FALCON_IRQMSET_RISCV_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_TRACE_FIELD is new LwU1;
    Lw_Csec_Falcon_Irqmset_Trace_Set :
        constant LW_CSEC_FALCON_IRQMSET_TRACE_FIELD
        := 16#0000_0001#;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_IRQMSET_REGISTER is
    record
        F_Gptmr    : LW_CSEC_FALCON_IRQMSET_GPTMR_FIELD;
        F_Wdtmr    : LW_CSEC_FALCON_IRQMSET_WDTMR_FIELD;
        F_Mthd    : LW_CSEC_FALCON_IRQMSET_MTHD_FIELD;
        F_Ctxsw    : LW_CSEC_FALCON_IRQMSET_CTXSW_FIELD;
        F_Halt    : LW_CSEC_FALCON_IRQMSET_HALT_FIELD;
        F_Exterr    : LW_CSEC_FALCON_IRQMSET_EXTERR_FIELD;
        F_Swgen0    : LW_CSEC_FALCON_IRQMSET_SWGEN0_FIELD;
        F_Swgen1    : LW_CSEC_FALCON_IRQMSET_SWGEN1_FIELD;
        F_Ext_Extirq1    : LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ1_FIELD;
        F_Ext_Extirq2    : LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ2_FIELD;
        F_Ext_Extirq3    : LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ3_FIELD;
        F_Ext_Extirq4    : LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ4_FIELD;
        F_Ext_Extirq5    : LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ5_FIELD;
        F_Ext_Extirq6    : LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ6_FIELD;
        F_Ext_Extirq7    : LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ7_FIELD;
        F_Ext_Extirq8    : LW_CSEC_FALCON_IRQMSET_EXT_EXTIRQ8_FIELD;
        F_Dma    : LW_CSEC_FALCON_IRQMSET_DMA_FIELD;
        F_Sha    : LW_CSEC_FALCON_IRQMSET_SHA_FIELD;
        F_Memerr    : LW_CSEC_FALCON_IRQMSET_MEMERR_FIELD;
        F_Falcon    : LW_CSEC_FALCON_IRQMSET_FALCON_FIELD;
        F_Riscv    : LW_CSEC_FALCON_IRQMSET_RISCV_FIELD;
        F_Trace    : LW_CSEC_FALCON_IRQMSET_TRACE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_IRQMSET_REGISTER use
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
        F_Falcon at 0 range 19 .. 19;
        F_Riscv at 0 range 20 .. 20;
        F_Trace at 0 range 21 .. 21;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Irqdest    : constant CSB_ADDR := 16#0000_0700#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_HOST_GPTMR_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Host_Gptmr_Init,
        Lw_Csec_Falcon_Irqdest_Host_Gptmr_Host
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_HOST_GPTMR_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Host_Gptmr_Init => 0,
        Lw_Csec_Falcon_Irqdest_Host_Gptmr_Host => 1
    );

    Lw_Csec_Falcon_Irqdest_Host_Gptmr_Falcon :
        constant LW_CSEC_FALCON_IRQDEST_HOST_GPTMR_FIELD
        := Lw_Csec_Falcon_Irqdest_Host_Gptmr_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_HOST_WDTMR_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Host_Wdtmr_Init,
        Lw_Csec_Falcon_Irqdest_Host_Wdtmr_Host
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_HOST_WDTMR_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Host_Wdtmr_Init => 0,
        Lw_Csec_Falcon_Irqdest_Host_Wdtmr_Host => 1
    );

    Lw_Csec_Falcon_Irqdest_Host_Wdtmr_Falcon :
        constant LW_CSEC_FALCON_IRQDEST_HOST_WDTMR_FIELD
        := Lw_Csec_Falcon_Irqdest_Host_Wdtmr_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_HOST_MTHD_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Host_Mthd_Init,
        Lw_Csec_Falcon_Irqdest_Host_Mthd_Host
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_HOST_MTHD_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Host_Mthd_Init => 0,
        Lw_Csec_Falcon_Irqdest_Host_Mthd_Host => 1
    );

    Lw_Csec_Falcon_Irqdest_Host_Mthd_Falcon :
        constant LW_CSEC_FALCON_IRQDEST_HOST_MTHD_FIELD
        := Lw_Csec_Falcon_Irqdest_Host_Mthd_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_HOST_CTXSW_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Host_Ctxsw_Init,
        Lw_Csec_Falcon_Irqdest_Host_Ctxsw_Host
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_HOST_CTXSW_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Host_Ctxsw_Init => 0,
        Lw_Csec_Falcon_Irqdest_Host_Ctxsw_Host => 1
    );

    Lw_Csec_Falcon_Irqdest_Host_Ctxsw_Falcon :
        constant LW_CSEC_FALCON_IRQDEST_HOST_CTXSW_FIELD
        := Lw_Csec_Falcon_Irqdest_Host_Ctxsw_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_HOST_HALT_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Host_Halt_Init,
        Lw_Csec_Falcon_Irqdest_Host_Halt_Host
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_HOST_HALT_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Host_Halt_Init => 0,
        Lw_Csec_Falcon_Irqdest_Host_Halt_Host => 1
    );

    Lw_Csec_Falcon_Irqdest_Host_Halt_Falcon :
        constant LW_CSEC_FALCON_IRQDEST_HOST_HALT_FIELD
        := Lw_Csec_Falcon_Irqdest_Host_Halt_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_HOST_EXTERR_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Host_Exterr_Init,
        Lw_Csec_Falcon_Irqdest_Host_Exterr_Host
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_HOST_EXTERR_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Host_Exterr_Init => 0,
        Lw_Csec_Falcon_Irqdest_Host_Exterr_Host => 1
    );

    Lw_Csec_Falcon_Irqdest_Host_Exterr_Falcon :
        constant LW_CSEC_FALCON_IRQDEST_HOST_EXTERR_FIELD
        := Lw_Csec_Falcon_Irqdest_Host_Exterr_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_HOST_SWGEN0_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Host_Swgen0_Init,
        Lw_Csec_Falcon_Irqdest_Host_Swgen0_Host
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_HOST_SWGEN0_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Host_Swgen0_Init => 0,
        Lw_Csec_Falcon_Irqdest_Host_Swgen0_Host => 1
    );

    Lw_Csec_Falcon_Irqdest_Host_Swgen0_Falcon :
        constant LW_CSEC_FALCON_IRQDEST_HOST_SWGEN0_FIELD
        := Lw_Csec_Falcon_Irqdest_Host_Swgen0_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_HOST_SWGEN1_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Host_Swgen1_Init,
        Lw_Csec_Falcon_Irqdest_Host_Swgen1_Host
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_HOST_SWGEN1_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Host_Swgen1_Init => 0,
        Lw_Csec_Falcon_Irqdest_Host_Swgen1_Host => 1
    );

    Lw_Csec_Falcon_Irqdest_Host_Swgen1_Falcon :
        constant LW_CSEC_FALCON_IRQDEST_HOST_SWGEN1_FIELD
        := Lw_Csec_Falcon_Irqdest_Host_Swgen1_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ1_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq1_Falcon,
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq1_Host
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ1_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq1_Falcon => 0,
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq1_Host => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ2_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq2_Falcon,
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq2_Host
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ2_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq2_Falcon => 0,
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq2_Host => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ3_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq3_Falcon,
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq3_Host
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ3_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq3_Falcon => 0,
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq3_Host => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ4_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq4_Falcon,
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq4_Host
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ4_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq4_Falcon => 0,
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq4_Host => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ5_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq5_Falcon,
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq5_Host
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ5_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq5_Falcon => 0,
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq5_Host => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ6_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq6_Falcon,
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq6_Host
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ6_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq6_Falcon => 0,
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq6_Host => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ7_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq7_Falcon,
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq7_Host
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ7_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq7_Falcon => 0,
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq7_Host => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ8_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq8_Falcon,
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq8_Host
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ8_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq8_Falcon => 0,
        Lw_Csec_Falcon_Irqdest_Host_Ext_Extirq8_Host => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_TARGET_GPTMR_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Target_Gptmr_Init,
        Lw_Csec_Falcon_Irqdest_Target_Gptmr_Falcon_Irq1
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_TARGET_GPTMR_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Target_Gptmr_Init => 0,
        Lw_Csec_Falcon_Irqdest_Target_Gptmr_Falcon_Irq1 => 1
    );

    Lw_Csec_Falcon_Irqdest_Target_Gptmr_Falcon_Irq0 :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_GPTMR_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Gptmr_Init;

    Lw_Csec_Falcon_Irqdest_Target_Gptmr_Host_Normal :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_GPTMR_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Gptmr_Init;

    Lw_Csec_Falcon_Irqdest_Target_Gptmr_Host_Nonstall :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_GPTMR_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Gptmr_Falcon_Irq1;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_TARGET_WDTMR_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Target_Wdtmr_Init,
        Lw_Csec_Falcon_Irqdest_Target_Wdtmr_Falcon_Irq1
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_TARGET_WDTMR_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Target_Wdtmr_Init => 0,
        Lw_Csec_Falcon_Irqdest_Target_Wdtmr_Falcon_Irq1 => 1
    );

    Lw_Csec_Falcon_Irqdest_Target_Wdtmr_Falcon_Irq0 :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_WDTMR_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Wdtmr_Init;

    Lw_Csec_Falcon_Irqdest_Target_Wdtmr_Host_Normal :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_WDTMR_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Wdtmr_Init;

    Lw_Csec_Falcon_Irqdest_Target_Wdtmr_Host_Nonstall :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_WDTMR_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Wdtmr_Falcon_Irq1;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_TARGET_MTHD_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Target_Mthd_Init,
        Lw_Csec_Falcon_Irqdest_Target_Mthd_Falcon_Irq1
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_TARGET_MTHD_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Target_Mthd_Init => 0,
        Lw_Csec_Falcon_Irqdest_Target_Mthd_Falcon_Irq1 => 1
    );

    Lw_Csec_Falcon_Irqdest_Target_Mthd_Falcon_Irq0 :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_MTHD_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Mthd_Init;

    Lw_Csec_Falcon_Irqdest_Target_Mthd_Host_Normal :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_MTHD_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Mthd_Init;

    Lw_Csec_Falcon_Irqdest_Target_Mthd_Host_Nonstall :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_MTHD_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Mthd_Falcon_Irq1;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_TARGET_CTXSW_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Target_Ctxsw_Init,
        Lw_Csec_Falcon_Irqdest_Target_Ctxsw_Falcon_Irq1
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_TARGET_CTXSW_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Target_Ctxsw_Init => 0,
        Lw_Csec_Falcon_Irqdest_Target_Ctxsw_Falcon_Irq1 => 1
    );

    Lw_Csec_Falcon_Irqdest_Target_Ctxsw_Falcon_Irq0 :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_CTXSW_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ctxsw_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ctxsw_Host_Normal :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_CTXSW_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ctxsw_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ctxsw_Host_Nonstall :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_CTXSW_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ctxsw_Falcon_Irq1;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_TARGET_HALT_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Target_Halt_Init,
        Lw_Csec_Falcon_Irqdest_Target_Halt_Falcon_Irq1
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_TARGET_HALT_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Target_Halt_Init => 0,
        Lw_Csec_Falcon_Irqdest_Target_Halt_Falcon_Irq1 => 1
    );

    Lw_Csec_Falcon_Irqdest_Target_Halt_Falcon_Irq0 :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_HALT_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Halt_Init;

    Lw_Csec_Falcon_Irqdest_Target_Halt_Host_Normal :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_HALT_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Halt_Init;

    Lw_Csec_Falcon_Irqdest_Target_Halt_Host_Nonstall :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_HALT_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Halt_Falcon_Irq1;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_TARGET_EXTERR_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Target_Exterr_Init,
        Lw_Csec_Falcon_Irqdest_Target_Exterr_Falcon_Irq1
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_TARGET_EXTERR_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Target_Exterr_Init => 0,
        Lw_Csec_Falcon_Irqdest_Target_Exterr_Falcon_Irq1 => 1
    );

    Lw_Csec_Falcon_Irqdest_Target_Exterr_Falcon_Irq0 :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXTERR_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Exterr_Init;

    Lw_Csec_Falcon_Irqdest_Target_Exterr_Host_Normal :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXTERR_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Exterr_Init;

    Lw_Csec_Falcon_Irqdest_Target_Exterr_Host_Nonstall :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXTERR_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Exterr_Falcon_Irq1;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_TARGET_SWGEN0_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Target_Swgen0_Init,
        Lw_Csec_Falcon_Irqdest_Target_Swgen0_Falcon_Irq1
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_TARGET_SWGEN0_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Target_Swgen0_Init => 0,
        Lw_Csec_Falcon_Irqdest_Target_Swgen0_Falcon_Irq1 => 1
    );

    Lw_Csec_Falcon_Irqdest_Target_Swgen0_Falcon_Irq0 :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_SWGEN0_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Swgen0_Init;

    Lw_Csec_Falcon_Irqdest_Target_Swgen0_Host_Normal :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_SWGEN0_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Swgen0_Init;

    Lw_Csec_Falcon_Irqdest_Target_Swgen0_Host_Nonstall :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_SWGEN0_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Swgen0_Falcon_Irq1;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_TARGET_SWGEN1_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Target_Swgen1_Init,
        Lw_Csec_Falcon_Irqdest_Target_Swgen1_Falcon_Irq1
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_TARGET_SWGEN1_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Target_Swgen1_Init => 0,
        Lw_Csec_Falcon_Irqdest_Target_Swgen1_Falcon_Irq1 => 1
    );

    Lw_Csec_Falcon_Irqdest_Target_Swgen1_Falcon_Irq0 :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_SWGEN1_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Swgen1_Init;

    Lw_Csec_Falcon_Irqdest_Target_Swgen1_Host_Normal :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_SWGEN1_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Swgen1_Init;

    Lw_Csec_Falcon_Irqdest_Target_Swgen1_Host_Nonstall :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_SWGEN1_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Swgen1_Falcon_Irq1;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ1_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq1_Init,
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq1_Falcon_Irq1
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ1_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq1_Init => 0,
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq1_Falcon_Irq1 => 1
    );

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq1_Falcon_Irq0 :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ1_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq1_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq1_Host_Normal :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ1_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq1_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq1_Host_Nonstall :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ1_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq1_Falcon_Irq1;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ2_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq2_Init,
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq2_Falcon_Irq1
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ2_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq2_Init => 0,
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq2_Falcon_Irq1 => 1
    );

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq2_Falcon_Irq0 :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ2_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq2_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq2_Host_Normal :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ2_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq2_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq2_Host_Nonstall :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ2_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq2_Falcon_Irq1;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ3_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq3_Init,
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq3_Falcon_Irq1
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ3_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq3_Init => 0,
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq3_Falcon_Irq1 => 1
    );

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq3_Falcon_Irq0 :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ3_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq3_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq3_Host_Normal :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ3_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq3_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq3_Host_Nonstall :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ3_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq3_Falcon_Irq1;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ4_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq4_Init,
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq4_Falcon_Irq1
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ4_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq4_Init => 0,
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq4_Falcon_Irq1 => 1
    );

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq4_Falcon_Irq0 :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ4_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq4_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq4_Host_Normal :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ4_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq4_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq4_Host_Nonstall :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ4_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq4_Falcon_Irq1;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ5_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq5_Init,
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq5_Falcon_Irq1
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ5_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq5_Init => 0,
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq5_Falcon_Irq1 => 1
    );

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq5_Falcon_Irq0 :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ5_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq5_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq5_Host_Normal :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ5_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq5_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq5_Host_Nonstall :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ5_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq5_Falcon_Irq1;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ6_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq6_Init,
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq6_Falcon_Irq1
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ6_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq6_Init => 0,
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq6_Falcon_Irq1 => 1
    );

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq6_Falcon_Irq0 :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ6_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq6_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq6_Host_Normal :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ6_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq6_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq6_Host_Nonstall :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ6_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq6_Falcon_Irq1;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ7_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq7_Init,
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq7_Falcon_Irq1
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ7_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq7_Init => 0,
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq7_Falcon_Irq1 => 1
    );

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq7_Falcon_Irq0 :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ7_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq7_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq7_Host_Normal :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ7_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq7_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq7_Host_Nonstall :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ7_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq7_Falcon_Irq1;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ8_FIELD is
    (
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq8_Init,
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq8_Falcon_Irq1
    ) with size => 1;

    for LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ8_FIELD use
    (
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq8_Init => 0,
        Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq8_Falcon_Irq1 => 1
    );

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq8_Falcon_Irq0 :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ8_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq8_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq8_Host_Normal :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ8_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq8_Init;

    Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq8_Host_Nonstall :
        constant LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ8_FIELD
        := Lw_Csec_Falcon_Irqdest_Target_Ext_Extirq8_Falcon_Irq1;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_IRQDEST_REGISTER is
    record
        F_Host_Gptmr    : LW_CSEC_FALCON_IRQDEST_HOST_GPTMR_FIELD;
        F_Host_Wdtmr    : LW_CSEC_FALCON_IRQDEST_HOST_WDTMR_FIELD;
        F_Host_Mthd    : LW_CSEC_FALCON_IRQDEST_HOST_MTHD_FIELD;
        F_Host_Ctxsw    : LW_CSEC_FALCON_IRQDEST_HOST_CTXSW_FIELD;
        F_Host_Halt    : LW_CSEC_FALCON_IRQDEST_HOST_HALT_FIELD;
        F_Host_Exterr    : LW_CSEC_FALCON_IRQDEST_HOST_EXTERR_FIELD;
        F_Host_Swgen0    : LW_CSEC_FALCON_IRQDEST_HOST_SWGEN0_FIELD;
        F_Host_Swgen1    : LW_CSEC_FALCON_IRQDEST_HOST_SWGEN1_FIELD;
        F_Host_Ext_Extirq1    : LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ1_FIELD;
        F_Host_Ext_Extirq2    : LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ2_FIELD;
        F_Host_Ext_Extirq3    : LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ3_FIELD;
        F_Host_Ext_Extirq4    : LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ4_FIELD;
        F_Host_Ext_Extirq5    : LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ5_FIELD;
        F_Host_Ext_Extirq6    : LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ6_FIELD;
        F_Host_Ext_Extirq7    : LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ7_FIELD;
        F_Host_Ext_Extirq8    : LW_CSEC_FALCON_IRQDEST_HOST_EXT_EXTIRQ8_FIELD;
        F_Target_Gptmr    : LW_CSEC_FALCON_IRQDEST_TARGET_GPTMR_FIELD;
        F_Target_Wdtmr    : LW_CSEC_FALCON_IRQDEST_TARGET_WDTMR_FIELD;
        F_Target_Mthd    : LW_CSEC_FALCON_IRQDEST_TARGET_MTHD_FIELD;
        F_Target_Ctxsw    : LW_CSEC_FALCON_IRQDEST_TARGET_CTXSW_FIELD;
        F_Target_Halt    : LW_CSEC_FALCON_IRQDEST_TARGET_HALT_FIELD;
        F_Target_Exterr    : LW_CSEC_FALCON_IRQDEST_TARGET_EXTERR_FIELD;
        F_Target_Swgen0    : LW_CSEC_FALCON_IRQDEST_TARGET_SWGEN0_FIELD;
        F_Target_Swgen1    : LW_CSEC_FALCON_IRQDEST_TARGET_SWGEN1_FIELD;
        F_Target_Ext_Extirq1    : LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ1_FIELD;
        F_Target_Ext_Extirq2    : LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ2_FIELD;
        F_Target_Ext_Extirq3    : LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ3_FIELD;
        F_Target_Ext_Extirq4    : LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ4_FIELD;
        F_Target_Ext_Extirq5    : LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ5_FIELD;
        F_Target_Ext_Extirq6    : LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ6_FIELD;
        F_Target_Ext_Extirq7    : LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ7_FIELD;
        F_Target_Ext_Extirq8    : LW_CSEC_FALCON_IRQDEST_TARGET_EXT_EXTIRQ8_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_IRQDEST_REGISTER use
    record
        F_Host_Gptmr at 0 range 0 .. 0;
        F_Host_Wdtmr at 0 range 1 .. 1;
        F_Host_Mthd at 0 range 2 .. 2;
        F_Host_Ctxsw at 0 range 3 .. 3;
        F_Host_Halt at 0 range 4 .. 4;
        F_Host_Exterr at 0 range 5 .. 5;
        F_Host_Swgen0 at 0 range 6 .. 6;
        F_Host_Swgen1 at 0 range 7 .. 7;
        F_Host_Ext_Extirq1 at 0 range 8 .. 8;
        F_Host_Ext_Extirq2 at 0 range 9 .. 9;
        F_Host_Ext_Extirq3 at 0 range 10 .. 10;
        F_Host_Ext_Extirq4 at 0 range 11 .. 11;
        F_Host_Ext_Extirq5 at 0 range 12 .. 12;
        F_Host_Ext_Extirq6 at 0 range 13 .. 13;
        F_Host_Ext_Extirq7 at 0 range 14 .. 14;
        F_Host_Ext_Extirq8 at 0 range 15 .. 15;
        F_Target_Gptmr at 0 range 16 .. 16;
        F_Target_Wdtmr at 0 range 17 .. 17;
        F_Target_Mthd at 0 range 18 .. 18;
        F_Target_Ctxsw at 0 range 19 .. 19;
        F_Target_Halt at 0 range 20 .. 20;
        F_Target_Exterr at 0 range 21 .. 21;
        F_Target_Swgen0 at 0 range 22 .. 22;
        F_Target_Swgen1 at 0 range 23 .. 23;
        F_Target_Ext_Extirq1 at 0 range 24 .. 24;
        F_Target_Ext_Extirq2 at 0 range 25 .. 25;
        F_Target_Ext_Extirq3 at 0 range 26 .. 26;
        F_Target_Ext_Extirq4 at 0 range 27 .. 27;
        F_Target_Ext_Extirq5 at 0 range 28 .. 28;
        F_Target_Ext_Extirq6 at 0 range 29 .. 29;
        F_Target_Ext_Extirq7 at 0 range 30 .. 30;
        F_Target_Ext_Extirq8 at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Irqscmask    : constant CSB_ADDR := 16#0000_3800#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQSCMASK_GPTMR_FIELD is
    (
        Lw_Csec_Falcon_Irqscmask_Gptmr_Disable,
        Lw_Csec_Falcon_Irqscmask_Gptmr_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IRQSCMASK_GPTMR_FIELD use
    (
        Lw_Csec_Falcon_Irqscmask_Gptmr_Disable => 0,
        Lw_Csec_Falcon_Irqscmask_Gptmr_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQSCMASK_WDTMR_FIELD is
    (
        Lw_Csec_Falcon_Irqscmask_Wdtmr_Disable,
        Lw_Csec_Falcon_Irqscmask_Wdtmr_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IRQSCMASK_WDTMR_FIELD use
    (
        Lw_Csec_Falcon_Irqscmask_Wdtmr_Disable => 0,
        Lw_Csec_Falcon_Irqscmask_Wdtmr_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQSCMASK_MTHD_FIELD is
    (
        Lw_Csec_Falcon_Irqscmask_Mthd_Disable,
        Lw_Csec_Falcon_Irqscmask_Mthd_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IRQSCMASK_MTHD_FIELD use
    (
        Lw_Csec_Falcon_Irqscmask_Mthd_Disable => 0,
        Lw_Csec_Falcon_Irqscmask_Mthd_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQSCMASK_CTXSW_FIELD is
    (
        Lw_Csec_Falcon_Irqscmask_Ctxsw_Disable,
        Lw_Csec_Falcon_Irqscmask_Ctxsw_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IRQSCMASK_CTXSW_FIELD use
    (
        Lw_Csec_Falcon_Irqscmask_Ctxsw_Disable => 0,
        Lw_Csec_Falcon_Irqscmask_Ctxsw_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQSCMASK_HALT_FIELD is
    (
        Lw_Csec_Falcon_Irqscmask_Halt_Disable,
        Lw_Csec_Falcon_Irqscmask_Halt_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IRQSCMASK_HALT_FIELD use
    (
        Lw_Csec_Falcon_Irqscmask_Halt_Disable => 0,
        Lw_Csec_Falcon_Irqscmask_Halt_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQSCMASK_EXTERR_FIELD is
    (
        Lw_Csec_Falcon_Irqscmask_Exterr_Disable,
        Lw_Csec_Falcon_Irqscmask_Exterr_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IRQSCMASK_EXTERR_FIELD use
    (
        Lw_Csec_Falcon_Irqscmask_Exterr_Disable => 0,
        Lw_Csec_Falcon_Irqscmask_Exterr_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQSCMASK_SWGEN0_FIELD is
    (
        Lw_Csec_Falcon_Irqscmask_Swgen0_Disable,
        Lw_Csec_Falcon_Irqscmask_Swgen0_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IRQSCMASK_SWGEN0_FIELD use
    (
        Lw_Csec_Falcon_Irqscmask_Swgen0_Disable => 0,
        Lw_Csec_Falcon_Irqscmask_Swgen0_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQSCMASK_SWGEN1_FIELD is
    (
        Lw_Csec_Falcon_Irqscmask_Swgen1_Disable,
        Lw_Csec_Falcon_Irqscmask_Swgen1_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IRQSCMASK_SWGEN1_FIELD use
    (
        Lw_Csec_Falcon_Irqscmask_Swgen1_Disable => 0,
        Lw_Csec_Falcon_Irqscmask_Swgen1_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQSCMASK_EXT_FIELD is
    (
        Lw_Csec_Falcon_Irqscmask_Ext_Disable,
        Lw_Csec_Falcon_Irqscmask_Ext_Enable
    ) with size => 8;

    for LW_CSEC_FALCON_IRQSCMASK_EXT_FIELD use
    (
        Lw_Csec_Falcon_Irqscmask_Ext_Disable => 0,
        Lw_Csec_Falcon_Irqscmask_Ext_Enable => 255
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQSCMASK_DMA_FIELD is
    (
        Lw_Csec_Falcon_Irqscmask_Dma_Disable,
        Lw_Csec_Falcon_Irqscmask_Dma_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IRQSCMASK_DMA_FIELD use
    (
        Lw_Csec_Falcon_Irqscmask_Dma_Disable => 0,
        Lw_Csec_Falcon_Irqscmask_Dma_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQSCMASK_SHA_FIELD is
    (
        Lw_Csec_Falcon_Irqscmask_Sha_Disable,
        Lw_Csec_Falcon_Irqscmask_Sha_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IRQSCMASK_SHA_FIELD use
    (
        Lw_Csec_Falcon_Irqscmask_Sha_Disable => 0,
        Lw_Csec_Falcon_Irqscmask_Sha_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQSCMASK_MEMERR_FIELD is
    (
        Lw_Csec_Falcon_Irqscmask_Memerr_Disable,
        Lw_Csec_Falcon_Irqscmask_Memerr_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IRQSCMASK_MEMERR_FIELD use
    (
        Lw_Csec_Falcon_Irqscmask_Memerr_Disable => 0,
        Lw_Csec_Falcon_Irqscmask_Memerr_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQSCMASK_FALCON_FIELD is
    (
        Lw_Csec_Falcon_Irqscmask_Falcon_Disable,
        Lw_Csec_Falcon_Irqscmask_Falcon_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IRQSCMASK_FALCON_FIELD use
    (
        Lw_Csec_Falcon_Irqscmask_Falcon_Disable => 0,
        Lw_Csec_Falcon_Irqscmask_Falcon_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQSCMASK_RISCV_FIELD is
    (
        Lw_Csec_Falcon_Irqscmask_Riscv_Disable,
        Lw_Csec_Falcon_Irqscmask_Riscv_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IRQSCMASK_RISCV_FIELD use
    (
        Lw_Csec_Falcon_Irqscmask_Riscv_Disable => 0,
        Lw_Csec_Falcon_Irqscmask_Riscv_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IRQSCMASK_TRACE_FIELD is
    (
        Lw_Csec_Falcon_Irqscmask_Trace_Disable,
        Lw_Csec_Falcon_Irqscmask_Trace_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IRQSCMASK_TRACE_FIELD use
    (
        Lw_Csec_Falcon_Irqscmask_Trace_Disable => 0,
        Lw_Csec_Falcon_Irqscmask_Trace_Enable => 1
    );

---------- Record Declaration ----------

    type LW_CSEC_FALCON_IRQSCMASK_REGISTER is
    record
        F_Gptmr    : LW_CSEC_FALCON_IRQSCMASK_GPTMR_FIELD;
        F_Wdtmr    : LW_CSEC_FALCON_IRQSCMASK_WDTMR_FIELD;
        F_Mthd    : LW_CSEC_FALCON_IRQSCMASK_MTHD_FIELD;
        F_Ctxsw    : LW_CSEC_FALCON_IRQSCMASK_CTXSW_FIELD;
        F_Halt    : LW_CSEC_FALCON_IRQSCMASK_HALT_FIELD;
        F_Exterr    : LW_CSEC_FALCON_IRQSCMASK_EXTERR_FIELD;
        F_Swgen0    : LW_CSEC_FALCON_IRQSCMASK_SWGEN0_FIELD;
        F_Swgen1    : LW_CSEC_FALCON_IRQSCMASK_SWGEN1_FIELD;
        F_Ext    : LW_CSEC_FALCON_IRQSCMASK_EXT_FIELD;
        F_Dma    : LW_CSEC_FALCON_IRQSCMASK_DMA_FIELD;
        F_Sha    : LW_CSEC_FALCON_IRQSCMASK_SHA_FIELD;
        F_Memerr    : LW_CSEC_FALCON_IRQSCMASK_MEMERR_FIELD;
        F_Falcon    : LW_CSEC_FALCON_IRQSCMASK_FALCON_FIELD;
        F_Riscv    : LW_CSEC_FALCON_IRQSCMASK_RISCV_FIELD;
        F_Trace    : LW_CSEC_FALCON_IRQSCMASK_TRACE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_IRQSCMASK_REGISTER use
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
        F_Falcon at 0 range 19 .. 19;
        F_Riscv at 0 range 20 .. 20;
        F_Trace at 0 range 21 .. 21;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Ptimer0    : constant CSB_ADDR := 16#0000_0B00#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_PTIMER0_VAL_FIELD is new LwU32;
---------- Record Declaration ----------

    type LW_CSEC_FALCON_PTIMER0_REGISTER is
    record
        F_Val    : LW_CSEC_FALCON_PTIMER0_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_PTIMER0_REGISTER use
    record
        F_Val at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Debug1    : constant CSB_ADDR := 16#0000_2400#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DEBUG1_MTHD_DRAIN_TIME_FIELD is new LwU16;
    Lw_Csec_Falcon_Debug1_Mthd_Drain_Time_Init :
        constant LW_CSEC_FALCON_DEBUG1_MTHD_DRAIN_TIME_FIELD
        := 16#0000_0040#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DEBUG1_CTXSW_MODE_FIELD is new LwU1;
    Lw_Csec_Falcon_Debug1_Ctxsw_Mode_Init :
        constant LW_CSEC_FALCON_DEBUG1_CTXSW_MODE_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_DEBUG1_TRACE_FORMAT_FIELD is
    (
        Lw_Csec_Falcon_Debug1_Trace_Format_Init,
        Lw_Csec_Falcon_Debug1_Trace_Format_Compressed
    ) with size => 1;

    for LW_CSEC_FALCON_DEBUG1_TRACE_FORMAT_FIELD use
    (
        Lw_Csec_Falcon_Debug1_Trace_Format_Init => 0,
        Lw_Csec_Falcon_Debug1_Trace_Format_Compressed => 1
    );

    Lw_Csec_Falcon_Debug1_Trace_Format_Uncompressed :
        constant LW_CSEC_FALCON_DEBUG1_TRACE_FORMAT_FIELD
        := Lw_Csec_Falcon_Debug1_Trace_Format_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_DEBUG1_CTXSW_MODE1_FIELD is
    (
        Lw_Csec_Falcon_Debug1_Ctxsw_Mode1_Init,
        Lw_Csec_Falcon_Debug1_Ctxsw_Mode1_Bypass_Idle_Checks
    ) with size => 1;

    for LW_CSEC_FALCON_DEBUG1_CTXSW_MODE1_FIELD use
    (
        Lw_Csec_Falcon_Debug1_Ctxsw_Mode1_Init => 0,
        Lw_Csec_Falcon_Debug1_Ctxsw_Mode1_Bypass_Idle_Checks => 1
    );

    Lw_Csec_Falcon_Debug1_Ctxsw_Mode1_Default :
        constant LW_CSEC_FALCON_DEBUG1_CTXSW_MODE1_FIELD
        := Lw_Csec_Falcon_Debug1_Ctxsw_Mode1_Init;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_DEBUG1_REGISTER is
    record
        F_Mthd_Drain_Time    : LW_CSEC_FALCON_DEBUG1_MTHD_DRAIN_TIME_FIELD;
        F_Ctxsw_Mode    : LW_CSEC_FALCON_DEBUG1_CTXSW_MODE_FIELD;
        F_Trace_Format    : LW_CSEC_FALCON_DEBUG1_TRACE_FORMAT_FIELD;
        F_Ctxsw_Mode1    : LW_CSEC_FALCON_DEBUG1_CTXSW_MODE1_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_DEBUG1_REGISTER use
    record
        F_Mthd_Drain_Time at 0 range 0 .. 15;
        F_Ctxsw_Mode at 0 range 16 .. 16;
        F_Trace_Format at 0 range 17 .. 17;
        F_Ctxsw_Mode1 at 0 range 18 .. 18;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Ibrkpt1    : constant CSB_ADDR := 16#0000_2600#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IBRKPT1_PC_FIELD is new LwU24;
    Lw_Csec_Falcon_Ibrkpt1_Pc_Init :
        constant LW_CSEC_FALCON_IBRKPT1_PC_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IBRKPT1_SUPPRESS_FIELD is
    (
        Lw_Csec_Falcon_Ibrkpt1_Suppress_Init,
        Lw_Csec_Falcon_Ibrkpt1_Suppress_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IBRKPT1_SUPPRESS_FIELD use
    (
        Lw_Csec_Falcon_Ibrkpt1_Suppress_Init => 0,
        Lw_Csec_Falcon_Ibrkpt1_Suppress_Enable => 1
    );

    Lw_Csec_Falcon_Ibrkpt1_Suppress_Disable :
        constant LW_CSEC_FALCON_IBRKPT1_SUPPRESS_FIELD
        := Lw_Csec_Falcon_Ibrkpt1_Suppress_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IBRKPT1_SKIP_FIELD is
    (
        Lw_Csec_Falcon_Ibrkpt1_Skip_Init,
        Lw_Csec_Falcon_Ibrkpt1_Skip_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IBRKPT1_SKIP_FIELD use
    (
        Lw_Csec_Falcon_Ibrkpt1_Skip_Init => 0,
        Lw_Csec_Falcon_Ibrkpt1_Skip_Enable => 1
    );

    Lw_Csec_Falcon_Ibrkpt1_Skip_Disable :
        constant LW_CSEC_FALCON_IBRKPT1_SKIP_FIELD
        := Lw_Csec_Falcon_Ibrkpt1_Skip_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IBRKPT1_EN_FIELD is
    (
        Lw_Csec_Falcon_Ibrkpt1_En_Init,
        Lw_Csec_Falcon_Ibrkpt1_En_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IBRKPT1_EN_FIELD use
    (
        Lw_Csec_Falcon_Ibrkpt1_En_Init => 0,
        Lw_Csec_Falcon_Ibrkpt1_En_Enable => 1
    );

    Lw_Csec_Falcon_Ibrkpt1_En_Disable :
        constant LW_CSEC_FALCON_IBRKPT1_EN_FIELD
        := Lw_Csec_Falcon_Ibrkpt1_En_Init;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_IBRKPT1_REGISTER is
    record
        F_Pc    : LW_CSEC_FALCON_IBRKPT1_PC_FIELD;
        F_Suppress    : LW_CSEC_FALCON_IBRKPT1_SUPPRESS_FIELD;
        F_Skip    : LW_CSEC_FALCON_IBRKPT1_SKIP_FIELD;
        F_En    : LW_CSEC_FALCON_IBRKPT1_EN_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_IBRKPT1_REGISTER use
    record
        F_Pc at 0 range 0 .. 23;
        F_Suppress at 0 range 29 .. 29;
        F_Skip at 0 range 30 .. 30;
        F_En at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Ibrkpt2    : constant CSB_ADDR := 16#0000_2700#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IBRKPT2_PC_FIELD is new LwU24;
    Lw_Csec_Falcon_Ibrkpt2_Pc_Init :
        constant LW_CSEC_FALCON_IBRKPT2_PC_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IBRKPT2_SUPPRESS_FIELD is
    (
        Lw_Csec_Falcon_Ibrkpt2_Suppress_Init,
        Lw_Csec_Falcon_Ibrkpt2_Suppress_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IBRKPT2_SUPPRESS_FIELD use
    (
        Lw_Csec_Falcon_Ibrkpt2_Suppress_Init => 0,
        Lw_Csec_Falcon_Ibrkpt2_Suppress_Enable => 1
    );

    Lw_Csec_Falcon_Ibrkpt2_Suppress_Disable :
        constant LW_CSEC_FALCON_IBRKPT2_SUPPRESS_FIELD
        := Lw_Csec_Falcon_Ibrkpt2_Suppress_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IBRKPT2_SKIP_FIELD is
    (
        Lw_Csec_Falcon_Ibrkpt2_Skip_Init,
        Lw_Csec_Falcon_Ibrkpt2_Skip_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IBRKPT2_SKIP_FIELD use
    (
        Lw_Csec_Falcon_Ibrkpt2_Skip_Init => 0,
        Lw_Csec_Falcon_Ibrkpt2_Skip_Enable => 1
    );

    Lw_Csec_Falcon_Ibrkpt2_Skip_Disable :
        constant LW_CSEC_FALCON_IBRKPT2_SKIP_FIELD
        := Lw_Csec_Falcon_Ibrkpt2_Skip_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IBRKPT2_EN_FIELD is
    (
        Lw_Csec_Falcon_Ibrkpt2_En_Init,
        Lw_Csec_Falcon_Ibrkpt2_En_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IBRKPT2_EN_FIELD use
    (
        Lw_Csec_Falcon_Ibrkpt2_En_Init => 0,
        Lw_Csec_Falcon_Ibrkpt2_En_Enable => 1
    );

    Lw_Csec_Falcon_Ibrkpt2_En_Disable :
        constant LW_CSEC_FALCON_IBRKPT2_EN_FIELD
        := Lw_Csec_Falcon_Ibrkpt2_En_Init;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_IBRKPT2_REGISTER is
    record
        F_Pc    : LW_CSEC_FALCON_IBRKPT2_PC_FIELD;
        F_Suppress    : LW_CSEC_FALCON_IBRKPT2_SUPPRESS_FIELD;
        F_Skip    : LW_CSEC_FALCON_IBRKPT2_SKIP_FIELD;
        F_En    : LW_CSEC_FALCON_IBRKPT2_EN_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_IBRKPT2_REGISTER use
    record
        F_Pc at 0 range 0 .. 23;
        F_Suppress at 0 range 29 .. 29;
        F_Skip at 0 range 30 .. 30;
        F_En at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Ibrkpt3    : constant CSB_ADDR := 16#0000_2C00#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IBRKPT3_PC_FIELD is new LwU24;
    Lw_Csec_Falcon_Ibrkpt3_Pc_Init :
        constant LW_CSEC_FALCON_IBRKPT3_PC_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IBRKPT3_SUPPRESS_FIELD is
    (
        Lw_Csec_Falcon_Ibrkpt3_Suppress_Init,
        Lw_Csec_Falcon_Ibrkpt3_Suppress_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IBRKPT3_SUPPRESS_FIELD use
    (
        Lw_Csec_Falcon_Ibrkpt3_Suppress_Init => 0,
        Lw_Csec_Falcon_Ibrkpt3_Suppress_Enable => 1
    );

    Lw_Csec_Falcon_Ibrkpt3_Suppress_Disable :
        constant LW_CSEC_FALCON_IBRKPT3_SUPPRESS_FIELD
        := Lw_Csec_Falcon_Ibrkpt3_Suppress_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IBRKPT3_SKIP_FIELD is
    (
        Lw_Csec_Falcon_Ibrkpt3_Skip_Init,
        Lw_Csec_Falcon_Ibrkpt3_Skip_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IBRKPT3_SKIP_FIELD use
    (
        Lw_Csec_Falcon_Ibrkpt3_Skip_Init => 0,
        Lw_Csec_Falcon_Ibrkpt3_Skip_Enable => 1
    );

    Lw_Csec_Falcon_Ibrkpt3_Skip_Disable :
        constant LW_CSEC_FALCON_IBRKPT3_SKIP_FIELD
        := Lw_Csec_Falcon_Ibrkpt3_Skip_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IBRKPT3_EN_FIELD is
    (
        Lw_Csec_Falcon_Ibrkpt3_En_Init,
        Lw_Csec_Falcon_Ibrkpt3_En_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IBRKPT3_EN_FIELD use
    (
        Lw_Csec_Falcon_Ibrkpt3_En_Init => 0,
        Lw_Csec_Falcon_Ibrkpt3_En_Enable => 1
    );

    Lw_Csec_Falcon_Ibrkpt3_En_Disable :
        constant LW_CSEC_FALCON_IBRKPT3_EN_FIELD
        := Lw_Csec_Falcon_Ibrkpt3_En_Init;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_IBRKPT3_REGISTER is
    record
        F_Pc    : LW_CSEC_FALCON_IBRKPT3_PC_FIELD;
        F_Suppress    : LW_CSEC_FALCON_IBRKPT3_SUPPRESS_FIELD;
        F_Skip    : LW_CSEC_FALCON_IBRKPT3_SKIP_FIELD;
        F_En    : LW_CSEC_FALCON_IBRKPT3_EN_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_IBRKPT3_REGISTER use
    record
        F_Pc at 0 range 0 .. 23;
        F_Suppress at 0 range 29 .. 29;
        F_Skip at 0 range 30 .. 30;
        F_En at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Ibrkpt4    : constant CSB_ADDR := 16#0000_2D00#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IBRKPT4_PC_FIELD is new LwU24;
    Lw_Csec_Falcon_Ibrkpt4_Pc_Init :
        constant LW_CSEC_FALCON_IBRKPT4_PC_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IBRKPT4_SUPPRESS_FIELD is
    (
        Lw_Csec_Falcon_Ibrkpt4_Suppress_Init,
        Lw_Csec_Falcon_Ibrkpt4_Suppress_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IBRKPT4_SUPPRESS_FIELD use
    (
        Lw_Csec_Falcon_Ibrkpt4_Suppress_Init => 0,
        Lw_Csec_Falcon_Ibrkpt4_Suppress_Enable => 1
    );

    Lw_Csec_Falcon_Ibrkpt4_Suppress_Disable :
        constant LW_CSEC_FALCON_IBRKPT4_SUPPRESS_FIELD
        := Lw_Csec_Falcon_Ibrkpt4_Suppress_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IBRKPT4_SKIP_FIELD is
    (
        Lw_Csec_Falcon_Ibrkpt4_Skip_Init,
        Lw_Csec_Falcon_Ibrkpt4_Skip_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IBRKPT4_SKIP_FIELD use
    (
        Lw_Csec_Falcon_Ibrkpt4_Skip_Init => 0,
        Lw_Csec_Falcon_Ibrkpt4_Skip_Enable => 1
    );

    Lw_Csec_Falcon_Ibrkpt4_Skip_Disable :
        constant LW_CSEC_FALCON_IBRKPT4_SKIP_FIELD
        := Lw_Csec_Falcon_Ibrkpt4_Skip_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IBRKPT4_EN_FIELD is
    (
        Lw_Csec_Falcon_Ibrkpt4_En_Init,
        Lw_Csec_Falcon_Ibrkpt4_En_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IBRKPT4_EN_FIELD use
    (
        Lw_Csec_Falcon_Ibrkpt4_En_Init => 0,
        Lw_Csec_Falcon_Ibrkpt4_En_Enable => 1
    );

    Lw_Csec_Falcon_Ibrkpt4_En_Disable :
        constant LW_CSEC_FALCON_IBRKPT4_EN_FIELD
        := Lw_Csec_Falcon_Ibrkpt4_En_Init;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_IBRKPT4_REGISTER is
    record
        F_Pc    : LW_CSEC_FALCON_IBRKPT4_PC_FIELD;
        F_Suppress    : LW_CSEC_FALCON_IBRKPT4_SUPPRESS_FIELD;
        F_Skip    : LW_CSEC_FALCON_IBRKPT4_SKIP_FIELD;
        F_En    : LW_CSEC_FALCON_IBRKPT4_EN_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_IBRKPT4_REGISTER use
    record
        F_Pc at 0 range 0 .. 23;
        F_Suppress at 0 range 29 .. 29;
        F_Skip at 0 range 30 .. 30;
        F_En at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Ibrkpt5    : constant CSB_ADDR := 16#0000_2E00#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IBRKPT5_PC_FIELD is new LwU24;
    Lw_Csec_Falcon_Ibrkpt5_Pc_Init :
        constant LW_CSEC_FALCON_IBRKPT5_PC_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IBRKPT5_SUPPRESS_FIELD is
    (
        Lw_Csec_Falcon_Ibrkpt5_Suppress_Init,
        Lw_Csec_Falcon_Ibrkpt5_Suppress_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IBRKPT5_SUPPRESS_FIELD use
    (
        Lw_Csec_Falcon_Ibrkpt5_Suppress_Init => 0,
        Lw_Csec_Falcon_Ibrkpt5_Suppress_Enable => 1
    );

    Lw_Csec_Falcon_Ibrkpt5_Suppress_Disable :
        constant LW_CSEC_FALCON_IBRKPT5_SUPPRESS_FIELD
        := Lw_Csec_Falcon_Ibrkpt5_Suppress_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IBRKPT5_SKIP_FIELD is
    (
        Lw_Csec_Falcon_Ibrkpt5_Skip_Init,
        Lw_Csec_Falcon_Ibrkpt5_Skip_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IBRKPT5_SKIP_FIELD use
    (
        Lw_Csec_Falcon_Ibrkpt5_Skip_Init => 0,
        Lw_Csec_Falcon_Ibrkpt5_Skip_Enable => 1
    );

    Lw_Csec_Falcon_Ibrkpt5_Skip_Disable :
        constant LW_CSEC_FALCON_IBRKPT5_SKIP_FIELD
        := Lw_Csec_Falcon_Ibrkpt5_Skip_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IBRKPT5_EN_FIELD is
    (
        Lw_Csec_Falcon_Ibrkpt5_En_Init,
        Lw_Csec_Falcon_Ibrkpt5_En_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_IBRKPT5_EN_FIELD use
    (
        Lw_Csec_Falcon_Ibrkpt5_En_Init => 0,
        Lw_Csec_Falcon_Ibrkpt5_En_Enable => 1
    );

    Lw_Csec_Falcon_Ibrkpt5_En_Disable :
        constant LW_CSEC_FALCON_IBRKPT5_EN_FIELD
        := Lw_Csec_Falcon_Ibrkpt5_En_Init;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_IBRKPT5_REGISTER is
    record
        F_Pc    : LW_CSEC_FALCON_IBRKPT5_PC_FIELD;
        F_Suppress    : LW_CSEC_FALCON_IBRKPT5_SUPPRESS_FIELD;
        F_Skip    : LW_CSEC_FALCON_IBRKPT5_SKIP_FIELD;
        F_En    : LW_CSEC_FALCON_IBRKPT5_EN_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_IBRKPT5_REGISTER use
    record
        F_Pc at 0 range 0 .. 23;
        F_Suppress at 0 range 29 .. 29;
        F_Skip at 0 range 30 .. 30;
        F_En at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Exci    : constant CSB_ADDR := 16#0000_3400#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_EXCI_EXPC_FIELD is new LwU20;
---------- Enum Declaration ----------

    type LW_CSEC_FALCON_EXCI_EXCAUSE_FIELD is
    (
        Lw_Csec_Falcon_Exci_Excause_Trap0,
        Lw_Csec_Falcon_Exci_Excause_Trap1,
        Lw_Csec_Falcon_Exci_Excause_Trap2,
        Lw_Csec_Falcon_Exci_Excause_Trap3,
        Lw_Csec_Falcon_Exci_Excause_Ill_Ins,
        Lw_Csec_Falcon_Exci_Excause_Ilw_Ins,
        Lw_Csec_Falcon_Exci_Excause_Miss_Ins,
        Lw_Csec_Falcon_Exci_Excause_Dhit_Ins,
        Lw_Csec_Falcon_Exci_Excause_Sp_Overflow,
        Lw_Csec_Falcon_Exci_Excause_Brkpt_Ins,
        Lw_Csec_Falcon_Exci_Excause_Dmem_Miss_Ins,
        Lw_Csec_Falcon_Exci_Excause_Dmem_Dhit_Ins,
        Lw_Csec_Falcon_Exci_Excause_Dmem_Pafault_Ins,
        Lw_Csec_Falcon_Exci_Excause_Dmem_Permission_Ins,
        Lw_Csec_Falcon_Exci_Excause_Brom_Call_Ins,
        Lw_Csec_Falcon_Exci_Excause_Kmem_Violation_Ins,
        Lw_Csec_Falcon_Exci_Excause_Bmem_Permission_Ins
    ) with size => 5;

    for LW_CSEC_FALCON_EXCI_EXCAUSE_FIELD use
    (
        Lw_Csec_Falcon_Exci_Excause_Trap0 => 0,
        Lw_Csec_Falcon_Exci_Excause_Trap1 => 1,
        Lw_Csec_Falcon_Exci_Excause_Trap2 => 2,
        Lw_Csec_Falcon_Exci_Excause_Trap3 => 3,
        Lw_Csec_Falcon_Exci_Excause_Ill_Ins => 8,
        Lw_Csec_Falcon_Exci_Excause_Ilw_Ins => 9,
        Lw_Csec_Falcon_Exci_Excause_Miss_Ins => 10,
        Lw_Csec_Falcon_Exci_Excause_Dhit_Ins => 11,
        Lw_Csec_Falcon_Exci_Excause_Sp_Overflow => 13,
        Lw_Csec_Falcon_Exci_Excause_Brkpt_Ins => 15,
        Lw_Csec_Falcon_Exci_Excause_Dmem_Miss_Ins => 16,
        Lw_Csec_Falcon_Exci_Excause_Dmem_Dhit_Ins => 17,
        Lw_Csec_Falcon_Exci_Excause_Dmem_Pafault_Ins => 18,
        Lw_Csec_Falcon_Exci_Excause_Dmem_Permission_Ins => 19,
        Lw_Csec_Falcon_Exci_Excause_Brom_Call_Ins => 21,
        Lw_Csec_Falcon_Exci_Excause_Kmem_Violation_Ins => 22,
        Lw_Csec_Falcon_Exci_Excause_Bmem_Permission_Ins => 23
    );

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_EXCI_EXPC_HIGH_FIELD is new LwU4;
---------- Record Declaration ----------

    type LW_CSEC_FALCON_EXCI_REGISTER is
    record
        F_Expc    : LW_CSEC_FALCON_EXCI_EXPC_FIELD;
        F_Excause    : LW_CSEC_FALCON_EXCI_EXCAUSE_FIELD;
        F_Expc_High    : LW_CSEC_FALCON_EXCI_EXPC_HIGH_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_EXCI_REGISTER use
    record
        F_Expc at 0 range 0 .. 19;
        F_Excause at 0 range 20 .. 24;
        F_Expc_High at 0 range 28 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Svec_Spr    : constant CSB_ADDR := 16#0000_3500#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_FALCON_SVEC_SPR_SIGPASS_FIELD is
    (
        Lw_Csec_Falcon_Svec_Spr_Sigpass_False,
        Lw_Csec_Falcon_Svec_Spr_Sigpass_True
    ) with size => 1;

    for LW_CSEC_FALCON_SVEC_SPR_SIGPASS_FIELD use
    (
        Lw_Csec_Falcon_Svec_Spr_Sigpass_False => 0,
        Lw_Csec_Falcon_Svec_Spr_Sigpass_True => 1
    );

---------- Record Declaration ----------

    type LW_CSEC_FALCON_SVEC_SPR_REGISTER is
    record
        F_Sigpass    : LW_CSEC_FALCON_SVEC_SPR_SIGPASS_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_SVEC_SPR_REGISTER use
    record
        F_Sigpass at 0 range 18 .. 18;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Cpuctl    : constant CSB_ADDR := 16#0000_4000#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_FALCON_CPUCTL_IILWAL_FIELD is
    (
        Lw_Csec_Falcon_Cpuctl_Iilwal_False,
        Lw_Csec_Falcon_Cpuctl_Iilwal_True
    ) with size => 1;

    for LW_CSEC_FALCON_CPUCTL_IILWAL_FIELD use
    (
        Lw_Csec_Falcon_Cpuctl_Iilwal_False => 0,
        Lw_Csec_Falcon_Cpuctl_Iilwal_True => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_CPUCTL_STARTCPU_FIELD is
    (
        Lw_Csec_Falcon_Cpuctl_Startcpu_False,
        Lw_Csec_Falcon_Cpuctl_Startcpu_True
    ) with size => 1;

    for LW_CSEC_FALCON_CPUCTL_STARTCPU_FIELD use
    (
        Lw_Csec_Falcon_Cpuctl_Startcpu_False => 0,
        Lw_Csec_Falcon_Cpuctl_Startcpu_True => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_CPUCTL_SRESET_FIELD is
    (
        Lw_Csec_Falcon_Cpuctl_Sreset_False,
        Lw_Csec_Falcon_Cpuctl_Sreset_True
    ) with size => 1;

    for LW_CSEC_FALCON_CPUCTL_SRESET_FIELD use
    (
        Lw_Csec_Falcon_Cpuctl_Sreset_False => 0,
        Lw_Csec_Falcon_Cpuctl_Sreset_True => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_CPUCTL_HRESET_FIELD is
    (
        Lw_Csec_Falcon_Cpuctl_Hreset_False,
        Lw_Csec_Falcon_Cpuctl_Hreset_True
    ) with size => 1;

    for LW_CSEC_FALCON_CPUCTL_HRESET_FIELD use
    (
        Lw_Csec_Falcon_Cpuctl_Hreset_False => 0,
        Lw_Csec_Falcon_Cpuctl_Hreset_True => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_CPUCTL_HALTED_FIELD is
    (
        Lw_Csec_Falcon_Cpuctl_Halted_False,
        Lw_Csec_Falcon_Cpuctl_Halted_True
    ) with size => 1;

    for LW_CSEC_FALCON_CPUCTL_HALTED_FIELD use
    (
        Lw_Csec_Falcon_Cpuctl_Halted_False => 0,
        Lw_Csec_Falcon_Cpuctl_Halted_True => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_CPUCTL_STOPPED_FIELD is
    (
        Lw_Csec_Falcon_Cpuctl_Stopped_False,
        Lw_Csec_Falcon_Cpuctl_Stopped_True
    ) with size => 1;

    for LW_CSEC_FALCON_CPUCTL_STOPPED_FIELD use
    (
        Lw_Csec_Falcon_Cpuctl_Stopped_False => 0,
        Lw_Csec_Falcon_Cpuctl_Stopped_True => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_CPUCTL_ALIAS_EN_FIELD is
    (
        Lw_Csec_Falcon_Cpuctl_Alias_En_False,
        Lw_Csec_Falcon_Cpuctl_Alias_En_True
    ) with size => 1;

    for LW_CSEC_FALCON_CPUCTL_ALIAS_EN_FIELD use
    (
        Lw_Csec_Falcon_Cpuctl_Alias_En_False => 0,
        Lw_Csec_Falcon_Cpuctl_Alias_En_True => 1
    );

    Lw_Csec_Falcon_Cpuctl_Alias_En_Init :
        constant LW_CSEC_FALCON_CPUCTL_ALIAS_EN_FIELD
        := Lw_Csec_Falcon_Cpuctl_Alias_En_False;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_CPUCTL_REGISTER is
    record
        F_Iilwal    : LW_CSEC_FALCON_CPUCTL_IILWAL_FIELD;
        F_Startcpu    : LW_CSEC_FALCON_CPUCTL_STARTCPU_FIELD;
        F_Sreset    : LW_CSEC_FALCON_CPUCTL_SRESET_FIELD;
        F_Hreset    : LW_CSEC_FALCON_CPUCTL_HRESET_FIELD;
        F_Halted    : LW_CSEC_FALCON_CPUCTL_HALTED_FIELD;
        F_Stopped    : LW_CSEC_FALCON_CPUCTL_STOPPED_FIELD;
        F_Alias_En    : LW_CSEC_FALCON_CPUCTL_ALIAS_EN_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_CPUCTL_REGISTER use
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
---------- Register Declaration ----------

    Lw_Csec_Falcon_Stackcfg    : constant CSB_ADDR := 16#0000_4E00#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_STACKCFG_BOTTOM_FIELD is new LwU24;
    Lw_Csec_Falcon_Stackcfg_Bottom_Init :
        constant LW_CSEC_FALCON_STACKCFG_BOTTOM_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_STACKCFG_SPEXC_FIELD is
    (
        Lw_Csec_Falcon_Stackcfg_Spexc_Init,
        Lw_Csec_Falcon_Stackcfg_Spexc_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_STACKCFG_SPEXC_FIELD use
    (
        Lw_Csec_Falcon_Stackcfg_Spexc_Init => 0,
        Lw_Csec_Falcon_Stackcfg_Spexc_Enable => 1
    );

    Lw_Csec_Falcon_Stackcfg_Spexc_Disable :
        constant LW_CSEC_FALCON_STACKCFG_SPEXC_FIELD
        := Lw_Csec_Falcon_Stackcfg_Spexc_Init;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_STACKCFG_REGISTER is
    record
        F_Bottom    : LW_CSEC_FALCON_STACKCFG_BOTTOM_FIELD;
        F_Spexc    : LW_CSEC_FALCON_STACKCFG_SPEXC_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_STACKCFG_REGISTER use
    record
        F_Bottom at 0 range 0 .. 23;
        F_Spexc at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Hwcfg1    : constant CSB_ADDR := 16#0000_4B00#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_FALCON_HWCFG1_CORE_REV_FIELD is
    (
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_1_0,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_2_0,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_3_0,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_4_0,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_5_0,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_Init,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_7_0
    ) with size => 4;

    for LW_CSEC_FALCON_HWCFG1_CORE_REV_FIELD use
    (
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_1_0 => 1,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_2_0 => 2,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_3_0 => 3,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_4_0 => 4,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_5_0 => 5,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_Init => 6,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_7_0 => 7
    );

    Lw_Csec_Falcon_Hwcfg1_Core_Rev_6_0 :
        constant LW_CSEC_FALCON_HWCFG1_CORE_REV_FIELD
        := Lw_Csec_Falcon_Hwcfg1_Core_Rev_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_HWCFG1_SELWRITY_MODEL_FIELD is
    (
        Lw_Csec_Falcon_Hwcfg1_Selwrity_Model_None,
        Lw_Csec_Falcon_Hwcfg1_Selwrity_Model_Light,
        Lw_Csec_Falcon_Hwcfg1_Selwrity_Model_Init
    ) with size => 2;

    for LW_CSEC_FALCON_HWCFG1_SELWRITY_MODEL_FIELD use
    (
        Lw_Csec_Falcon_Hwcfg1_Selwrity_Model_None => 0,
        Lw_Csec_Falcon_Hwcfg1_Selwrity_Model_Light => 2,
        Lw_Csec_Falcon_Hwcfg1_Selwrity_Model_Init => 3
    );

    Lw_Csec_Falcon_Hwcfg1_Selwrity_Model_Heavy :
        constant LW_CSEC_FALCON_HWCFG1_SELWRITY_MODEL_FIELD
        := Lw_Csec_Falcon_Hwcfg1_Selwrity_Model_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_HWCFG1_CORE_REV_SUBVERSION_FIELD is
    (
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_Subversion_Init,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_Subversion_1,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_Subversion_2,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_Subversion_3
    ) with size => 2;

    for LW_CSEC_FALCON_HWCFG1_CORE_REV_SUBVERSION_FIELD use
    (
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_Subversion_Init => 0,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_Subversion_1 => 1,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_Subversion_2 => 2,
        Lw_Csec_Falcon_Hwcfg1_Core_Rev_Subversion_3 => 3
    );

    Lw_Csec_Falcon_Hwcfg1_Core_Rev_Subversion_0 :
        constant LW_CSEC_FALCON_HWCFG1_CORE_REV_SUBVERSION_FIELD
        := Lw_Csec_Falcon_Hwcfg1_Core_Rev_Subversion_Init;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_HWCFG1_IMEM_PORTS_FIELD is new LwU4;
    Lw_Csec_Falcon_Hwcfg1_Imem_Ports_Init :
        constant LW_CSEC_FALCON_HWCFG1_IMEM_PORTS_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_HWCFG1_DMEM_PORTS_FIELD is new LwU4;
    Lw_Csec_Falcon_Hwcfg1_Dmem_Ports_Init :
        constant LW_CSEC_FALCON_HWCFG1_DMEM_PORTS_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_HWCFG1_TAG_WIDTH_FIELD is new LwU5;
    Lw_Csec_Falcon_Hwcfg1_Tag_Width_Init :
        constant LW_CSEC_FALCON_HWCFG1_TAG_WIDTH_FIELD
        := 16#0000_0010#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_HWCFG1_DMEM_TAG_WIDTH_FIELD is new LwU5;
    Lw_Csec_Falcon_Hwcfg1_Dmem_Tag_Width_Init :
        constant LW_CSEC_FALCON_HWCFG1_DMEM_TAG_WIDTH_FIELD
        := 16#0000_0010#;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_HWCFG1_DBG_PRIV_BUS_FIELD is
    (
        Lw_Csec_Falcon_Hwcfg1_Dbg_Priv_Bus_Disable,
        Lw_Csec_Falcon_Hwcfg1_Dbg_Priv_Bus_Init
    ) with size => 1;

    for LW_CSEC_FALCON_HWCFG1_DBG_PRIV_BUS_FIELD use
    (
        Lw_Csec_Falcon_Hwcfg1_Dbg_Priv_Bus_Disable => 0,
        Lw_Csec_Falcon_Hwcfg1_Dbg_Priv_Bus_Init => 1
    );

    Lw_Csec_Falcon_Hwcfg1_Dbg_Priv_Bus_Enable :
        constant LW_CSEC_FALCON_HWCFG1_DBG_PRIV_BUS_FIELD
        := Lw_Csec_Falcon_Hwcfg1_Dbg_Priv_Bus_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_HWCFG1_CSB_SIZE_16M_FIELD is
    (
        Lw_Csec_Falcon_Hwcfg1_Csb_Size_16M_Init,
        Lw_Csec_Falcon_Hwcfg1_Csb_Size_16M_True
    ) with size => 1;

    for LW_CSEC_FALCON_HWCFG1_CSB_SIZE_16M_FIELD use
    (
        Lw_Csec_Falcon_Hwcfg1_Csb_Size_16M_Init => 0,
        Lw_Csec_Falcon_Hwcfg1_Csb_Size_16M_True => 1
    );

    Lw_Csec_Falcon_Hwcfg1_Csb_Size_16M_False :
        constant LW_CSEC_FALCON_HWCFG1_CSB_SIZE_16M_FIELD
        := Lw_Csec_Falcon_Hwcfg1_Csb_Size_16M_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_HWCFG1_PRIV_DIRECT_FIELD is
    (
        Lw_Csec_Falcon_Hwcfg1_Priv_Direct_Init,
        Lw_Csec_Falcon_Hwcfg1_Priv_Direct_True
    ) with size => 1;

    for LW_CSEC_FALCON_HWCFG1_PRIV_DIRECT_FIELD use
    (
        Lw_Csec_Falcon_Hwcfg1_Priv_Direct_Init => 0,
        Lw_Csec_Falcon_Hwcfg1_Priv_Direct_True => 1
    );

    Lw_Csec_Falcon_Hwcfg1_Priv_Direct_False :
        constant LW_CSEC_FALCON_HWCFG1_PRIV_DIRECT_FIELD
        := Lw_Csec_Falcon_Hwcfg1_Priv_Direct_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_HWCFG1_DMEM_APERTURES_FIELD is
    (
        Lw_Csec_Falcon_Hwcfg1_Dmem_Apertures_Disable,
        Lw_Csec_Falcon_Hwcfg1_Dmem_Apertures_Init
    ) with size => 1;

    for LW_CSEC_FALCON_HWCFG1_DMEM_APERTURES_FIELD use
    (
        Lw_Csec_Falcon_Hwcfg1_Dmem_Apertures_Disable => 0,
        Lw_Csec_Falcon_Hwcfg1_Dmem_Apertures_Init => 1
    );

    Lw_Csec_Falcon_Hwcfg1_Dmem_Apertures_Enable :
        constant LW_CSEC_FALCON_HWCFG1_DMEM_APERTURES_FIELD
        := Lw_Csec_Falcon_Hwcfg1_Dmem_Apertures_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_HWCFG1_IMEM_AUTOFILL_FIELD is
    (
        Lw_Csec_Falcon_Hwcfg1_Imem_Autofill_Disable,
        Lw_Csec_Falcon_Hwcfg1_Imem_Autofill_Init
    ) with size => 1;

    for LW_CSEC_FALCON_HWCFG1_IMEM_AUTOFILL_FIELD use
    (
        Lw_Csec_Falcon_Hwcfg1_Imem_Autofill_Disable => 0,
        Lw_Csec_Falcon_Hwcfg1_Imem_Autofill_Init => 1
    );

    Lw_Csec_Falcon_Hwcfg1_Imem_Autofill_Enable :
        constant LW_CSEC_FALCON_HWCFG1_IMEM_AUTOFILL_FIELD
        := Lw_Csec_Falcon_Hwcfg1_Imem_Autofill_Init;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_HWCFG1_REGISTER is
    record
        F_Core_Rev    : LW_CSEC_FALCON_HWCFG1_CORE_REV_FIELD;
        F_Selwrity_Model    : LW_CSEC_FALCON_HWCFG1_SELWRITY_MODEL_FIELD;
        F_Core_Rev_Subversion    : LW_CSEC_FALCON_HWCFG1_CORE_REV_SUBVERSION_FIELD;
        F_Imem_Ports    : LW_CSEC_FALCON_HWCFG1_IMEM_PORTS_FIELD;
        F_Dmem_Ports    : LW_CSEC_FALCON_HWCFG1_DMEM_PORTS_FIELD;
        F_Tag_Width    : LW_CSEC_FALCON_HWCFG1_TAG_WIDTH_FIELD;
        F_Dmem_Tag_Width    : LW_CSEC_FALCON_HWCFG1_DMEM_TAG_WIDTH_FIELD;
        F_Dbg_Priv_Bus    : LW_CSEC_FALCON_HWCFG1_DBG_PRIV_BUS_FIELD;
        F_Csb_Size_16M    : LW_CSEC_FALCON_HWCFG1_CSB_SIZE_16M_FIELD;
        F_Priv_Direct    : LW_CSEC_FALCON_HWCFG1_PRIV_DIRECT_FIELD;
        F_Dmem_Apertures    : LW_CSEC_FALCON_HWCFG1_DMEM_APERTURES_FIELD;
        F_Imem_Autofill    : LW_CSEC_FALCON_HWCFG1_IMEM_AUTOFILL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_HWCFG1_REGISTER use
    record
        F_Core_Rev at 0 range 0 .. 3;
        F_Selwrity_Model at 0 range 4 .. 5;
        F_Core_Rev_Subversion at 0 range 6 .. 7;
        F_Imem_Ports at 0 range 8 .. 11;
        F_Dmem_Ports at 0 range 12 .. 15;
        F_Tag_Width at 0 range 16 .. 20;
        F_Dmem_Tag_Width at 0 range 21 .. 25;
        F_Dbg_Priv_Bus at 0 range 27 .. 27;
        F_Csb_Size_16M at 0 range 28 .. 28;
        F_Priv_Direct at 0 range 29 .. 29;
        F_Dmem_Apertures at 0 range 30 .. 30;
        F_Imem_Autofill at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Traceidx    : constant CSB_ADDR := 16#0000_5200#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_TRACEIDX_IDX_FIELD is new LwU8;
    Lw_Csec_Falcon_Traceidx_Idx_Init :
        constant LW_CSEC_FALCON_TRACEIDX_IDX_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_TRACEIDX_MAXIDX_FIELD is new LwU8;
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_TRACEIDX_CNT_FIELD is new LwU8;
    Lw_Csec_Falcon_Traceidx_Cnt_Init :
        constant LW_CSEC_FALCON_TRACEIDX_CNT_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_TRACEIDX_REGISTER is
    record
        F_Idx    : LW_CSEC_FALCON_TRACEIDX_IDX_FIELD;
        F_Maxidx    : LW_CSEC_FALCON_TRACEIDX_MAXIDX_FIELD;
        F_Cnt    : LW_CSEC_FALCON_TRACEIDX_CNT_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_TRACEIDX_REGISTER use
    record
        F_Idx at 0 range 0 .. 7;
        F_Maxidx at 0 range 16 .. 23;
        F_Cnt at 0 range 24 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Tracepc    : constant CSB_ADDR := 16#0000_5300#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_TRACEPC_PC_FIELD is new LwU24;
---------- Record Declaration ----------

    type LW_CSEC_FALCON_TRACEPC_REGISTER is
    record
        F_Pc    : LW_CSEC_FALCON_TRACEPC_PC_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_TRACEPC_REGISTER use
    record
        F_Pc at 0 range 0 .. 23;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Dmemapert    : constant CSB_ADDR := 16#0000_5900#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DMEMAPERT_TIME_OUT_FIELD is new LwU8;
    Lw_Csec_Falcon_Dmemapert_Time_Out_Init :
        constant LW_CSEC_FALCON_DMEMAPERT_TIME_OUT_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DMEMAPERT_TIME_UNIT_FIELD is new LwU4;
    Lw_Csec_Falcon_Dmemapert_Time_Unit_Init :
        constant LW_CSEC_FALCON_DMEMAPERT_TIME_UNIT_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_DMEMAPERT_ENABLE_FIELD is
    (
        Lw_Csec_Falcon_Dmemapert_Enable_Init,
        Lw_Csec_Falcon_Dmemapert_Enable_True
    ) with size => 1;

    for LW_CSEC_FALCON_DMEMAPERT_ENABLE_FIELD use
    (
        Lw_Csec_Falcon_Dmemapert_Enable_Init => 0,
        Lw_Csec_Falcon_Dmemapert_Enable_True => 1
    );

    Lw_Csec_Falcon_Dmemapert_Enable_False :
        constant LW_CSEC_FALCON_DMEMAPERT_ENABLE_FIELD
        := Lw_Csec_Falcon_Dmemapert_Enable_Init;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DMEMAPERT_LDSTQ_NUM_FIELD is new LwU3;
---------- Record Declaration ----------

    type LW_CSEC_FALCON_DMEMAPERT_REGISTER is
    record
        F_Time_Out    : LW_CSEC_FALCON_DMEMAPERT_TIME_OUT_FIELD;
        F_Time_Unit    : LW_CSEC_FALCON_DMEMAPERT_TIME_UNIT_FIELD;
        F_Enable    : LW_CSEC_FALCON_DMEMAPERT_ENABLE_FIELD;
        F_Ldstq_Num    : LW_CSEC_FALCON_DMEMAPERT_LDSTQ_NUM_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_DMEMAPERT_REGISTER use
    record
        F_Time_Out at 0 range 0 .. 7;
        F_Time_Unit at 0 range 8 .. 11;
        F_Enable at 0 range 16 .. 16;
        F_Ldstq_Num at 0 range 17 .. 19;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Exterraddr    : constant CSB_ADDR := 16#0000_5A00#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_EXTERRADDR_ADDR_FIELD is new LwU32;
---------- Record Declaration ----------

    type LW_CSEC_FALCON_EXTERRADDR_REGISTER is
    record
        F_Addr    : LW_CSEC_FALCON_EXTERRADDR_ADDR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_EXTERRADDR_REGISTER use
    record
        F_Addr at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Exterrstat    : constant CSB_ADDR := 16#0000_5B00#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_EXTERRSTAT_PC_FIELD is new LwU24;
---------- Enum Declaration ----------

    type LW_CSEC_FALCON_EXTERRSTAT_STAT_FIELD is
    (
        Lw_Csec_Falcon_Exterrstat_Stat_Ack_Pos,
        Lw_Csec_Falcon_Exterrstat_Stat_Ack_Tout
    ) with size => 4;

    for LW_CSEC_FALCON_EXTERRSTAT_STAT_FIELD use
    (
        Lw_Csec_Falcon_Exterrstat_Stat_Ack_Pos => 0,
        Lw_Csec_Falcon_Exterrstat_Stat_Ack_Tout => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_EXTERRSTAT_VALID_FIELD is
    (
        Lw_Csec_Falcon_Exterrstat_Valid_Init,
        Lw_Csec_Falcon_Exterrstat_Valid_True
    ) with size => 1;

    for LW_CSEC_FALCON_EXTERRSTAT_VALID_FIELD use
    (
        Lw_Csec_Falcon_Exterrstat_Valid_Init => 0,
        Lw_Csec_Falcon_Exterrstat_Valid_True => 1
    );

    Lw_Csec_Falcon_Exterrstat_Valid_False :
        constant LW_CSEC_FALCON_EXTERRSTAT_VALID_FIELD
        := Lw_Csec_Falcon_Exterrstat_Valid_Init;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_EXTERRSTAT_REGISTER is
    record
        F_Pc    : LW_CSEC_FALCON_EXTERRSTAT_PC_FIELD;
        F_Stat    : LW_CSEC_FALCON_EXTERRSTAT_STAT_FIELD;
        F_Valid    : LW_CSEC_FALCON_EXTERRSTAT_VALID_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_EXTERRSTAT_REGISTER use
    record
        F_Pc at 0 range 0 .. 23;
        F_Stat at 0 range 24 .. 27;
        F_Valid at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Engid    : constant CSB_ADDR := 16#0000_4F00#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_ENGID_INSTID_FIELD is new LwU8;
    Lw_Csec_Falcon_Engid_Instid_Init :
        constant LW_CSEC_FALCON_ENGID_INSTID_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_ENGID_FAMILYID_FIELD is
    (
        Lw_Csec_Falcon_Engid_Familyid_Sec,
        Lw_Csec_Falcon_Engid_Familyid_Dpu,
        Lw_Csec_Falcon_Engid_Familyid_Lwdec,
        Lw_Csec_Falcon_Engid_Familyid_Pwr_Pmu,
        Lw_Csec_Falcon_Engid_Familyid_Fbfaclon,
        Lw_Csec_Falcon_Engid_Familyid_Lwenc,
        Lw_Csec_Falcon_Engid_Familyid_Gpccs,
        Lw_Csec_Falcon_Engid_Familyid_Fecs,
        Lw_Csec_Falcon_Engid_Familyid_Minion,
        Lw_Csec_Falcon_Engid_Familyid_Xusb,
        Lw_Csec_Falcon_Engid_Familyid_Gsp,
        Lw_Csec_Falcon_Engid_Familyid_Lwjpg
    ) with size => 8;

    for LW_CSEC_FALCON_ENGID_FAMILYID_FIELD use
    (
        Lw_Csec_Falcon_Engid_Familyid_Sec => 1,
        Lw_Csec_Falcon_Engid_Familyid_Dpu => 2,
        Lw_Csec_Falcon_Engid_Familyid_Lwdec => 3,
        Lw_Csec_Falcon_Engid_Familyid_Pwr_Pmu => 4,
        Lw_Csec_Falcon_Engid_Familyid_Fbfaclon => 5,
        Lw_Csec_Falcon_Engid_Familyid_Lwenc => 6,
        Lw_Csec_Falcon_Engid_Familyid_Gpccs => 7,
        Lw_Csec_Falcon_Engid_Familyid_Fecs => 8,
        Lw_Csec_Falcon_Engid_Familyid_Minion => 9,
        Lw_Csec_Falcon_Engid_Familyid_Xusb => 10,
        Lw_Csec_Falcon_Engid_Familyid_Gsp => 11,
        Lw_Csec_Falcon_Engid_Familyid_Lwjpg => 12
    );

---------- Record Declaration ----------

    type LW_CSEC_FALCON_ENGID_REGISTER is
    record
        F_Instid    : LW_CSEC_FALCON_ENGID_INSTID_FIELD;
        F_Familyid    : LW_CSEC_FALCON_ENGID_FAMILYID_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_ENGID_REGISTER use
    record
        F_Instid at 0 range 0 .. 7;
        F_Familyid at 0 range 8 .. 15;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Csberrstat    : constant CSB_ADDR := 16#0000_9100#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_CSBERRSTAT_PC_FIELD is new LwU24;
---------- Enum Declaration ----------

    type LW_CSEC_FALCON_CSBERRSTAT_ENABLE_FIELD is
    (
        Lw_Csec_Falcon_Csberrstat_Enable_False,
        Lw_Csec_Falcon_Csberrstat_Enable_Init
    ) with size => 1;

    for LW_CSEC_FALCON_CSBERRSTAT_ENABLE_FIELD use
    (
        Lw_Csec_Falcon_Csberrstat_Enable_False => 0,
        Lw_Csec_Falcon_Csberrstat_Enable_Init => 1
    );

    Lw_Csec_Falcon_Csberrstat_Enable_True :
        constant LW_CSEC_FALCON_CSBERRSTAT_ENABLE_FIELD
        := Lw_Csec_Falcon_Csberrstat_Enable_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_CSBERRSTAT_VALID_FIELD is
    (
        Lw_Csec_Falcon_Csberrstat_Valid_Init,
        Lw_Csec_Falcon_Csberrstat_Valid_True
    ) with size => 1;

    for LW_CSEC_FALCON_CSBERRSTAT_VALID_FIELD use
    (
        Lw_Csec_Falcon_Csberrstat_Valid_Init => 0,
        Lw_Csec_Falcon_Csberrstat_Valid_True => 1
    );

    Lw_Csec_Falcon_Csberrstat_Valid_False :
        constant LW_CSEC_FALCON_CSBERRSTAT_VALID_FIELD
        := Lw_Csec_Falcon_Csberrstat_Valid_Init;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_CSBERRSTAT_REGISTER is
    record
        F_Pc    : LW_CSEC_FALCON_CSBERRSTAT_PC_FIELD;
        F_Enable    : LW_CSEC_FALCON_CSBERRSTAT_ENABLE_FIELD;
        F_Valid    : LW_CSEC_FALCON_CSBERRSTAT_VALID_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_CSBERRSTAT_REGISTER use
    record
        F_Pc at 0 range 0 .. 23;
        F_Enable at 0 range 30 .. 30;
        F_Valid at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Imemc    : constant CSB_ADDR := 16#0000_6000#;

    LW_CSEC_FALCON_IMEMC_SIZE_1 : constant := 4;
    subtype LW_CSEC_FALCON_IMEMC_INDEX is LwU32 range 0 .. LW_CSEC_FALCON_IMEMC_SIZE_1-1;

    function Lw_Csec_Falcon_Imemc_Addr( Index : LW_CSEC_FALCON_IMEMC_INDEX )
    return CSB_ADDR is
        (CSB_ADDR(LwU32(Lw_Csec_Falcon_Imemc) + (LwU32(Index)*1024)))
    with Inline_Always,
    Global => null,
    Depends => ( Lw_Csec_Falcon_Imemc_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IMEMC_OFFS_FIELD is new LwU6;
    Lw_Csec_Falcon_Imemc_Offs_Init :
        constant LW_CSEC_FALCON_IMEMC_OFFS_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IMEMC_BLK_FIELD is new LwU8;
    Lw_Csec_Falcon_Imemc_Blk_Init :
        constant LW_CSEC_FALCON_IMEMC_BLK_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IMEMC_AINCW_FIELD is
    (
        Lw_Csec_Falcon_Imemc_Aincw_Init,
        Lw_Csec_Falcon_Imemc_Aincw_True
    ) with size => 1;

    for LW_CSEC_FALCON_IMEMC_AINCW_FIELD use
    (
        Lw_Csec_Falcon_Imemc_Aincw_Init => 0,
        Lw_Csec_Falcon_Imemc_Aincw_True => 1
    );

    Lw_Csec_Falcon_Imemc_Aincw_False :
        constant LW_CSEC_FALCON_IMEMC_AINCW_FIELD
        := Lw_Csec_Falcon_Imemc_Aincw_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IMEMC_AINCR_FIELD is
    (
        Lw_Csec_Falcon_Imemc_Aincr_Init,
        Lw_Csec_Falcon_Imemc_Aincr_True
    ) with size => 1;

    for LW_CSEC_FALCON_IMEMC_AINCR_FIELD use
    (
        Lw_Csec_Falcon_Imemc_Aincr_Init => 0,
        Lw_Csec_Falcon_Imemc_Aincr_True => 1
    );

    Lw_Csec_Falcon_Imemc_Aincr_False :
        constant LW_CSEC_FALCON_IMEMC_AINCR_FIELD
        := Lw_Csec_Falcon_Imemc_Aincr_Init;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IMEMC_SELWRE_FIELD is new LwU1;
    Lw_Csec_Falcon_Imemc_Selwre_Init :
        constant LW_CSEC_FALCON_IMEMC_SELWRE_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IMEMC_SEC_ATOMIC_FIELD is
    (
        Lw_Csec_Falcon_Imemc_Sec_Atomic_False,
        Lw_Csec_Falcon_Imemc_Sec_Atomic_True
    ) with size => 1;

    for LW_CSEC_FALCON_IMEMC_SEC_ATOMIC_FIELD use
    (
        Lw_Csec_Falcon_Imemc_Sec_Atomic_False => 0,
        Lw_Csec_Falcon_Imemc_Sec_Atomic_True => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IMEMC_SEC_WR_VIO_FIELD is
    (
        Lw_Csec_Falcon_Imemc_Sec_Wr_Vio_False,
        Lw_Csec_Falcon_Imemc_Sec_Wr_Vio_True
    ) with size => 1;

    for LW_CSEC_FALCON_IMEMC_SEC_WR_VIO_FIELD use
    (
        Lw_Csec_Falcon_Imemc_Sec_Wr_Vio_False => 0,
        Lw_Csec_Falcon_Imemc_Sec_Wr_Vio_True => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_IMEMC_SEC_LOCK_FIELD is
    (
        Lw_Csec_Falcon_Imemc_Sec_Lock_False,
        Lw_Csec_Falcon_Imemc_Sec_Lock_True
    ) with size => 1;

    for LW_CSEC_FALCON_IMEMC_SEC_LOCK_FIELD use
    (
        Lw_Csec_Falcon_Imemc_Sec_Lock_False => 0,
        Lw_Csec_Falcon_Imemc_Sec_Lock_True => 1
    );

---------- Record Declaration ----------

    type LW_CSEC_FALCON_IMEMC_REGISTER is
    record
        F_Offs    : LW_CSEC_FALCON_IMEMC_OFFS_FIELD;
        F_Blk    : LW_CSEC_FALCON_IMEMC_BLK_FIELD;
        F_Aincw    : LW_CSEC_FALCON_IMEMC_AINCW_FIELD;
        F_Aincr    : LW_CSEC_FALCON_IMEMC_AINCR_FIELD;
        F_Selwre    : LW_CSEC_FALCON_IMEMC_SELWRE_FIELD;
        F_Sec_Atomic    : LW_CSEC_FALCON_IMEMC_SEC_ATOMIC_FIELD;
        F_Sec_Wr_Vio    : LW_CSEC_FALCON_IMEMC_SEC_WR_VIO_FIELD;
        F_Sec_Lock    : LW_CSEC_FALCON_IMEMC_SEC_LOCK_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_IMEMC_REGISTER use
    record
        F_Offs at 0 range 2 .. 7;
        F_Blk at 0 range 8 .. 15;
        F_Aincw at 0 range 24 .. 24;
        F_Aincr at 0 range 25 .. 25;
        F_Selwre at 0 range 28 .. 28;
        F_Sec_Atomic at 0 range 29 .. 29;
        F_Sec_Wr_Vio at 0 range 30 .. 30;
        F_Sec_Lock at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Imemd    : constant CSB_ADDR := 16#0000_6100#;

    LW_CSEC_FALCON_IMEMD_SIZE_1 : constant := 4;
    subtype LW_CSEC_FALCON_IMEMD_INDEX is LwU32 range 0 .. LW_CSEC_FALCON_IMEMD_SIZE_1-1;

    function Lw_Csec_Falcon_Imemd_Addr( Index : LW_CSEC_FALCON_IMEMD_INDEX )
    return CSB_ADDR is
        (CSB_ADDR(LwU32(Lw_Csec_Falcon_Imemd) + (LwU32(Index)*1024)))
    with Inline_Always,
    Global => null,
    Depends => ( Lw_Csec_Falcon_Imemd_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IMEMD_DATA_FIELD is new LwU32;
---------- Record Declaration ----------

    type LW_CSEC_FALCON_IMEMD_REGISTER is
    record
        F_Data    : LW_CSEC_FALCON_IMEMD_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_IMEMD_REGISTER use
    record
        F_Data at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Imemt    : constant CSB_ADDR := 16#0000_6200#;

    LW_CSEC_FALCON_IMEMT_SIZE_1 : constant := 4;
    subtype LW_CSEC_FALCON_IMEMT_INDEX is LwU32 range 0 .. LW_CSEC_FALCON_IMEMT_SIZE_1-1;

    function Lw_Csec_Falcon_Imemt_Addr( Index : LW_CSEC_FALCON_IMEMT_INDEX )
    return CSB_ADDR is
        (CSB_ADDR(LwU32(Lw_Csec_Falcon_Imemt) + (LwU32(Index)*1024)))
    with Inline_Always,
    Global => null,
    Depends => ( Lw_Csec_Falcon_Imemt_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_IMEMT_TAG_FIELD is new LwU16;
---------- Record Declaration ----------

    type LW_CSEC_FALCON_IMEMT_REGISTER is
    record
        F_Tag    : LW_CSEC_FALCON_IMEMT_TAG_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_IMEMT_REGISTER use
    record
        F_Tag at 0 range 0 .. 15;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Dmemc    : constant CSB_ADDR := 16#0000_7000#;

    LW_CSEC_FALCON_DMEMC_SIZE_1 : constant := 8;
    subtype LW_CSEC_FALCON_DMEMC_INDEX is LwU32 range 0 .. LW_CSEC_FALCON_DMEMC_SIZE_1-1;

    function Lw_Csec_Falcon_Dmemc_Addr( Index : LW_CSEC_FALCON_DMEMC_INDEX )
    return CSB_ADDR is
        (CSB_ADDR(LwU32(Lw_Csec_Falcon_Dmemc) + (LwU32(Index)*512)))
    with Inline_Always,
    Global => null,
    Depends => ( Lw_Csec_Falcon_Dmemc_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DMEMC_ADDRESS_FIELD is new LwU24;
    Lw_Csec_Falcon_Dmemc_Address_Init :
        constant LW_CSEC_FALCON_DMEMC_ADDRESS_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DMEMC_OFFS_FIELD is new LwU6;
    Lw_Csec_Falcon_Dmemc_Offs_Init :
        constant LW_CSEC_FALCON_DMEMC_OFFS_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DMEMC_BLK_FIELD is new LwU8;
    Lw_Csec_Falcon_Dmemc_Blk_Init :
        constant LW_CSEC_FALCON_DMEMC_BLK_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_DMEMC_AINCW_FIELD is
    (
        Lw_Csec_Falcon_Dmemc_Aincw_Init,
        Lw_Csec_Falcon_Dmemc_Aincw_True
    ) with size => 1;

    for LW_CSEC_FALCON_DMEMC_AINCW_FIELD use
    (
        Lw_Csec_Falcon_Dmemc_Aincw_Init => 0,
        Lw_Csec_Falcon_Dmemc_Aincw_True => 1
    );

    Lw_Csec_Falcon_Dmemc_Aincw_False :
        constant LW_CSEC_FALCON_DMEMC_AINCW_FIELD
        := Lw_Csec_Falcon_Dmemc_Aincw_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_DMEMC_AINCR_FIELD is
    (
        Lw_Csec_Falcon_Dmemc_Aincr_Init,
        Lw_Csec_Falcon_Dmemc_Aincr_True
    ) with size => 1;

    for LW_CSEC_FALCON_DMEMC_AINCR_FIELD use
    (
        Lw_Csec_Falcon_Dmemc_Aincr_Init => 0,
        Lw_Csec_Falcon_Dmemc_Aincr_True => 1
    );

    Lw_Csec_Falcon_Dmemc_Aincr_False :
        constant LW_CSEC_FALCON_DMEMC_AINCR_FIELD
        := Lw_Csec_Falcon_Dmemc_Aincr_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_DMEMC_SETTAG_FIELD is
    (
        Lw_Csec_Falcon_Dmemc_Settag_Init,
        Lw_Csec_Falcon_Dmemc_Settag_True
    ) with size => 1;

    for LW_CSEC_FALCON_DMEMC_SETTAG_FIELD use
    (
        Lw_Csec_Falcon_Dmemc_Settag_Init => 0,
        Lw_Csec_Falcon_Dmemc_Settag_True => 1
    );

    Lw_Csec_Falcon_Dmemc_Settag_False :
        constant LW_CSEC_FALCON_DMEMC_SETTAG_FIELD
        := Lw_Csec_Falcon_Dmemc_Settag_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_DMEMC_SETLVL_FIELD is
    (
        Lw_Csec_Falcon_Dmemc_Setlvl_Init,
        Lw_Csec_Falcon_Dmemc_Setlvl_True
    ) with size => 1;

    for LW_CSEC_FALCON_DMEMC_SETLVL_FIELD use
    (
        Lw_Csec_Falcon_Dmemc_Setlvl_Init => 0,
        Lw_Csec_Falcon_Dmemc_Setlvl_True => 1
    );

    Lw_Csec_Falcon_Dmemc_Setlvl_False :
        constant LW_CSEC_FALCON_DMEMC_SETLVL_FIELD
        := Lw_Csec_Falcon_Dmemc_Setlvl_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_DMEMC_VA_FIELD is
    (
        Lw_Csec_Falcon_Dmemc_Va_Init,
        Lw_Csec_Falcon_Dmemc_Va_True
    ) with size => 1;

    for LW_CSEC_FALCON_DMEMC_VA_FIELD use
    (
        Lw_Csec_Falcon_Dmemc_Va_Init => 0,
        Lw_Csec_Falcon_Dmemc_Va_True => 1
    );

    Lw_Csec_Falcon_Dmemc_Va_False :
        constant LW_CSEC_FALCON_DMEMC_VA_FIELD
        := Lw_Csec_Falcon_Dmemc_Va_Init;

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_DMEMC_MISS_FIELD is
    (
        Lw_Csec_Falcon_Dmemc_Miss_False,
        Lw_Csec_Falcon_Dmemc_Miss_True
    ) with size => 1;

    for LW_CSEC_FALCON_DMEMC_MISS_FIELD use
    (
        Lw_Csec_Falcon_Dmemc_Miss_False => 0,
        Lw_Csec_Falcon_Dmemc_Miss_True => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_DMEMC_MULTIHIT_FIELD is
    (
        Lw_Csec_Falcon_Dmemc_Multihit_False,
        Lw_Csec_Falcon_Dmemc_Multihit_True
    ) with size => 1;

    for LW_CSEC_FALCON_DMEMC_MULTIHIT_FIELD use
    (
        Lw_Csec_Falcon_Dmemc_Multihit_False => 0,
        Lw_Csec_Falcon_Dmemc_Multihit_True => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_DMEMC_LVLERR_FIELD is
    (
        Lw_Csec_Falcon_Dmemc_Lvlerr_False,
        Lw_Csec_Falcon_Dmemc_Lvlerr_True
    ) with size => 1;

    for LW_CSEC_FALCON_DMEMC_LVLERR_FIELD use
    (
        Lw_Csec_Falcon_Dmemc_Lvlerr_False => 0,
        Lw_Csec_Falcon_Dmemc_Lvlerr_True => 1
    );

---------- Record Declaration ----------

    type LW_CSEC_FALCON_DMEMC_REGISTER_0 is
    record
        F_Address    : LW_CSEC_FALCON_DMEMC_ADDRESS_FIELD;
        F_Aincw    : LW_CSEC_FALCON_DMEMC_AINCW_FIELD;
        F_Aincr    : LW_CSEC_FALCON_DMEMC_AINCR_FIELD;
        F_Settag    : LW_CSEC_FALCON_DMEMC_SETTAG_FIELD;
        F_Setlvl    : LW_CSEC_FALCON_DMEMC_SETLVL_FIELD;
        F_Va    : LW_CSEC_FALCON_DMEMC_VA_FIELD;
        F_Miss    : LW_CSEC_FALCON_DMEMC_MISS_FIELD;
        F_Multihit    : LW_CSEC_FALCON_DMEMC_MULTIHIT_FIELD;
        F_Lvlerr    : LW_CSEC_FALCON_DMEMC_LVLERR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_DMEMC_REGISTER_0 use
    record
        F_Address at 0 range 0 .. 23;
        F_Aincw at 0 range 24 .. 24;
        F_Aincr at 0 range 25 .. 25;
        F_Settag at 0 range 26 .. 26;
        F_Setlvl at 0 range 27 .. 27;
        F_Va at 0 range 28 .. 28;
        F_Miss at 0 range 29 .. 29;
        F_Multihit at 0 range 30 .. 30;
        F_Lvlerr at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
    type LW_CSEC_FALCON_DMEMC_REGISTER_1 is
    record
        F_Offs    : LW_CSEC_FALCON_DMEMC_OFFS_FIELD;
        F_Blk    : LW_CSEC_FALCON_DMEMC_BLK_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_DMEMC_REGISTER_1 use
    record
        F_Offs at 0 range 2 .. 7;
        F_Blk at 0 range 8 .. 15;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Dmemd    : constant CSB_ADDR := 16#0000_7100#;

    LW_CSEC_FALCON_DMEMD_SIZE_1 : constant := 8;
    subtype LW_CSEC_FALCON_DMEMD_INDEX is LwU32 range 0 .. LW_CSEC_FALCON_DMEMD_SIZE_1-1;

    function Lw_Csec_Falcon_Dmemd_Addr( Index : LW_CSEC_FALCON_DMEMD_INDEX )
    return CSB_ADDR is
        (CSB_ADDR(LwU32(Lw_Csec_Falcon_Dmemd) + (LwU32(Index)*512)))
    with Inline_Always,
    Global => null,
    Depends => ( Lw_Csec_Falcon_Dmemd_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DMEMD_DATA_FIELD is new LwU32;
---------- Record Declaration ----------

    type LW_CSEC_FALCON_DMEMD_REGISTER is
    record
        F_Data    : LW_CSEC_FALCON_DMEMD_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_DMEMD_REGISTER use
    record
        F_Data at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Doc_Ctrl    : constant CSB_ADDR := 16#0000_D000#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DOC_CTRL_COUNT_FIELD is new LwU8;
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DOC_CTRL_RESET_FIELD is new LwU1;
    Lw_Csec_Falcon_Doc_Ctrl_Reset_Init :
        constant LW_CSEC_FALCON_DOC_CTRL_RESET_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DOC_CTRL_STOP_FIELD is new LwU1;
    Lw_Csec_Falcon_Doc_Ctrl_Stop_Init :
        constant LW_CSEC_FALCON_DOC_CTRL_STOP_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DOC_CTRL_EMPTY_FIELD is new LwU1;
    Lw_Csec_Falcon_Doc_Ctrl_Empty_Init :
        constant LW_CSEC_FALCON_DOC_CTRL_EMPTY_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DOC_CTRL_WR_FINISHED_FIELD is new LwU1;
    Lw_Csec_Falcon_Doc_Ctrl_Wr_Finished_Init :
        constant LW_CSEC_FALCON_DOC_CTRL_WR_FINISHED_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DOC_CTRL_RD_FINISHED_FIELD is new LwU1;
    Lw_Csec_Falcon_Doc_Ctrl_Rd_Finished_Init :
        constant LW_CSEC_FALCON_DOC_CTRL_RD_FINISHED_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DOC_CTRL_WR_ERROR_FIELD is new LwU1;
    Lw_Csec_Falcon_Doc_Ctrl_Wr_Error_Init :
        constant LW_CSEC_FALCON_DOC_CTRL_WR_ERROR_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DOC_CTRL_RD_ERROR_FIELD is new LwU1;
    Lw_Csec_Falcon_Doc_Ctrl_Rd_Error_Init :
        constant LW_CSEC_FALCON_DOC_CTRL_RD_ERROR_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DOC_CTRL_PROTOCOL_ERROR_FIELD is new LwU1;
    Lw_Csec_Falcon_Doc_Ctrl_Protocol_Error_Init :
        constant LW_CSEC_FALCON_DOC_CTRL_PROTOCOL_ERROR_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_DOC_CTRL_REGISTER is
    record
        F_Count    : LW_CSEC_FALCON_DOC_CTRL_COUNT_FIELD;
        F_Reset    : LW_CSEC_FALCON_DOC_CTRL_RESET_FIELD;
        F_Stop    : LW_CSEC_FALCON_DOC_CTRL_STOP_FIELD;
        F_Empty    : LW_CSEC_FALCON_DOC_CTRL_EMPTY_FIELD;
        F_Wr_Finished    : LW_CSEC_FALCON_DOC_CTRL_WR_FINISHED_FIELD;
        F_Rd_Finished    : LW_CSEC_FALCON_DOC_CTRL_RD_FINISHED_FIELD;
        F_Wr_Error    : LW_CSEC_FALCON_DOC_CTRL_WR_ERROR_FIELD;
        F_Rd_Error    : LW_CSEC_FALCON_DOC_CTRL_RD_ERROR_FIELD;
        F_Protocol_Error    : LW_CSEC_FALCON_DOC_CTRL_PROTOCOL_ERROR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_DOC_CTRL_REGISTER use
    record
        F_Count at 0 range 0 .. 7;
        F_Reset at 0 range 16 .. 16;
        F_Stop at 0 range 17 .. 17;
        F_Empty at 0 range 18 .. 18;
        F_Wr_Finished at 0 range 19 .. 19;
        F_Rd_Finished at 0 range 20 .. 20;
        F_Wr_Error at 0 range 21 .. 21;
        F_Rd_Error at 0 range 22 .. 22;
        F_Protocol_Error at 0 range 23 .. 23;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Doc_D0    : constant CSB_ADDR := 16#0000_D100#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DOC_D0_DATA_FIELD is new LwU32;
---------- Record Declaration ----------

    type LW_CSEC_FALCON_DOC_D0_REGISTER is
    record
        F_Data    : LW_CSEC_FALCON_DOC_D0_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_DOC_D0_REGISTER use
    record
        F_Data at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Doc_D1    : constant CSB_ADDR := 16#0000_D200#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DOC_D1_DATA_FIELD is new LwU32;
---------- Record Declaration ----------

    type LW_CSEC_FALCON_DOC_D1_REGISTER is
    record
        F_Data    : LW_CSEC_FALCON_DOC_D1_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_DOC_D1_REGISTER use
    record
        F_Data at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Dic_Ctrl    : constant CSB_ADDR := 16#0000_D400#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DIC_CTRL_COUNT_FIELD is new LwU8;
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DIC_CTRL_RESET_FIELD is new LwU1;
    Lw_Csec_Falcon_Dic_Ctrl_Reset_Init :
        constant LW_CSEC_FALCON_DIC_CTRL_RESET_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DIC_CTRL_VALID_FIELD is new LwU1;
    Lw_Csec_Falcon_Dic_Ctrl_Valid_Init :
        constant LW_CSEC_FALCON_DIC_CTRL_VALID_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DIC_CTRL_POP_FIELD is new LwU1;
---------- Record Declaration ----------

    type LW_CSEC_FALCON_DIC_CTRL_REGISTER is
    record
        F_Count    : LW_CSEC_FALCON_DIC_CTRL_COUNT_FIELD;
        F_Reset    : LW_CSEC_FALCON_DIC_CTRL_RESET_FIELD;
        F_Valid    : LW_CSEC_FALCON_DIC_CTRL_VALID_FIELD;
        F_Pop    : LW_CSEC_FALCON_DIC_CTRL_POP_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_DIC_CTRL_REGISTER use
    record
        F_Count at 0 range 0 .. 7;
        F_Reset at 0 range 16 .. 16;
        F_Valid at 0 range 19 .. 19;
        F_Pop at 0 range 20 .. 20;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Dic_D0    : constant CSB_ADDR := 16#0000_D500#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_DIC_D0_DATA_FIELD is new LwU32;
---------- Record Declaration ----------

    type LW_CSEC_FALCON_DIC_D0_REGISTER is
    record
        F_Data    : LW_CSEC_FALCON_DIC_D0_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_DIC_D0_REGISTER use
    record
        F_Data at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Mod_Sel    : constant CSB_ADDR := 16#0004_6000#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_FALCON_MOD_SEL_ALGO_FIELD is
    (
        Lw_Csec_Falcon_Mod_Sel_Algo_Aes,
        Lw_Csec_Falcon_Mod_Sel_Algo_Rsa3K
    ) with size => 8;

    for LW_CSEC_FALCON_MOD_SEL_ALGO_FIELD use
    (
        Lw_Csec_Falcon_Mod_Sel_Algo_Aes => 0,
        Lw_Csec_Falcon_Mod_Sel_Algo_Rsa3K => 1
    );

---------- Record Declaration ----------

    type LW_CSEC_FALCON_MOD_SEL_REGISTER is
    record
        F_Algo    : LW_CSEC_FALCON_MOD_SEL_ALGO_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_MOD_SEL_REGISTER use
    record
        F_Algo at 0 range 0 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Vhrcfg_Base    : constant CSB_ADDR := 16#0004_6400#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_FALCON_VHRCFG_BASE_VAL_FIELD is
    (
        Lw_Csec_Falcon_Vhrcfg_Base_Val_Deft,
        Lw_Csec_Falcon_Vhrcfg_Base_Val_Sec,
        Lw_Csec_Falcon_Vhrcfg_Base_Val_Pwr_Pmu,
        Lw_Csec_Falcon_Vhrcfg_Base_Val_Lwdec,
        Lw_Csec_Falcon_Vhrcfg_Base_Val_Gsp
    ) with size => 32;

    for LW_CSEC_FALCON_VHRCFG_BASE_VAL_FIELD use
    (
        Lw_Csec_Falcon_Vhrcfg_Base_Val_Deft => 0,
        Lw_Csec_Falcon_Vhrcfg_Base_Val_Sec => 33280,
        Lw_Csec_Falcon_Vhrcfg_Base_Val_Pwr_Pmu => 33776,
        Lw_Csec_Falcon_Vhrcfg_Base_Val_Lwdec => 34272,
        Lw_Csec_Falcon_Vhrcfg_Base_Val_Gsp => 34528
    );

---------- Record Declaration ----------

    type LW_CSEC_FALCON_VHRCFG_BASE_REGISTER is
    record
        F_Val    : LW_CSEC_FALCON_VHRCFG_BASE_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_VHRCFG_BASE_REGISTER use
    record
        F_Val at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Vhrcfg_Entnum    : constant CSB_ADDR := 16#0004_7A00#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_FALCON_VHRCFG_ENTNUM_VAL_FIELD is
    (
        Lw_Csec_Falcon_Vhrcfg_Entnum_Val_Deft,
        Lw_Csec_Falcon_Vhrcfg_Entnum_Val_Gsp,
        Lw_Csec_Falcon_Vhrcfg_Entnum_Val_Lwdec,
        Lw_Csec_Falcon_Vhrcfg_Entnum_Val_Sec
    ) with size => 32;

    for LW_CSEC_FALCON_VHRCFG_ENTNUM_VAL_FIELD use
    (
        Lw_Csec_Falcon_Vhrcfg_Entnum_Val_Deft => 0,
        Lw_Csec_Falcon_Vhrcfg_Entnum_Val_Gsp => 3,
        Lw_Csec_Falcon_Vhrcfg_Entnum_Val_Lwdec => 5,
        Lw_Csec_Falcon_Vhrcfg_Entnum_Val_Sec => 10
    );

    Lw_Csec_Falcon_Vhrcfg_Entnum_Val_Pwr_Pmu :
        constant LW_CSEC_FALCON_VHRCFG_ENTNUM_VAL_FIELD
        := Lw_Csec_Falcon_Vhrcfg_Entnum_Val_Sec;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_VHRCFG_ENTNUM_REGISTER is
    record
        F_Val    : LW_CSEC_FALCON_VHRCFG_ENTNUM_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_VHRCFG_ENTNUM_REGISTER use
    record
        F_Val at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Brom_Lwrr_Ucode_Id    : constant CSB_ADDR := 16#0004_6600#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_FALCON_BROM_LWRR_UCODE_ID_VAL_FIELD is
    (
        Lw_Csec_Falcon_Brom_Lwrr_Ucode_Id_Val_Deft,
        Lw_Csec_Falcon_Brom_Lwrr_Ucode_Id_Val_Pub,
        Lw_Csec_Falcon_Brom_Lwrr_Ucode_Id_Val_Max
    ) with size => 8;

    for LW_CSEC_FALCON_BROM_LWRR_UCODE_ID_VAL_FIELD use
    (
        Lw_Csec_Falcon_Brom_Lwrr_Ucode_Id_Val_Deft => 0,
        Lw_Csec_Falcon_Brom_Lwrr_Ucode_Id_Val_Pub => 5,
        Lw_Csec_Falcon_Brom_Lwrr_Ucode_Id_Val_Max => 16
    );

---------- Record Declaration ----------

    type LW_CSEC_FALCON_BROM_LWRR_UCODE_ID_REGISTER is
    record
        F_Val    : LW_CSEC_FALCON_BROM_LWRR_UCODE_ID_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_BROM_LWRR_UCODE_ID_REGISTER use
    record
        F_Val at 0 range 0 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Brom_Engidmask    : constant CSB_ADDR := 16#0004_6700#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_FALCON_BROM_ENGIDMASK_SEC_FIELD is
    (
        Lw_Csec_Falcon_Brom_Engidmask_Sec_Disabled,
        Lw_Csec_Falcon_Brom_Engidmask_Sec_Enabled
    ) with size => 1;

    for LW_CSEC_FALCON_BROM_ENGIDMASK_SEC_FIELD use
    (
        Lw_Csec_Falcon_Brom_Engidmask_Sec_Disabled => 0,
        Lw_Csec_Falcon_Brom_Engidmask_Sec_Enabled => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_BROM_ENGIDMASK_DPU_FIELD is
    (
        Lw_Csec_Falcon_Brom_Engidmask_Dpu_Disabled,
        Lw_Csec_Falcon_Brom_Engidmask_Dpu_Enabled
    ) with size => 1;

    for LW_CSEC_FALCON_BROM_ENGIDMASK_DPU_FIELD use
    (
        Lw_Csec_Falcon_Brom_Engidmask_Dpu_Disabled => 0,
        Lw_Csec_Falcon_Brom_Engidmask_Dpu_Enabled => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_BROM_ENGIDMASK_LWDEC_FIELD is
    (
        Lw_Csec_Falcon_Brom_Engidmask_Lwdec_Disabled,
        Lw_Csec_Falcon_Brom_Engidmask_Lwdec_Enabled
    ) with size => 1;

    for LW_CSEC_FALCON_BROM_ENGIDMASK_LWDEC_FIELD use
    (
        Lw_Csec_Falcon_Brom_Engidmask_Lwdec_Disabled => 0,
        Lw_Csec_Falcon_Brom_Engidmask_Lwdec_Enabled => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_BROM_ENGIDMASK_PMU_FIELD is
    (
        Lw_Csec_Falcon_Brom_Engidmask_Pmu_Disabled,
        Lw_Csec_Falcon_Brom_Engidmask_Pmu_Enabled
    ) with size => 1;

    for LW_CSEC_FALCON_BROM_ENGIDMASK_PMU_FIELD use
    (
        Lw_Csec_Falcon_Brom_Engidmask_Pmu_Disabled => 0,
        Lw_Csec_Falcon_Brom_Engidmask_Pmu_Enabled => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_BROM_ENGIDMASK_FBFALCON_FIELD is
    (
        Lw_Csec_Falcon_Brom_Engidmask_Fbfalcon_Disabled,
        Lw_Csec_Falcon_Brom_Engidmask_Fbfalcon_Enabled
    ) with size => 1;

    for LW_CSEC_FALCON_BROM_ENGIDMASK_FBFALCON_FIELD use
    (
        Lw_Csec_Falcon_Brom_Engidmask_Fbfalcon_Disabled => 0,
        Lw_Csec_Falcon_Brom_Engidmask_Fbfalcon_Enabled => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_BROM_ENGIDMASK_LWENC_FIELD is
    (
        Lw_Csec_Falcon_Brom_Engidmask_Lwenc_Disabled,
        Lw_Csec_Falcon_Brom_Engidmask_Lwenc_Enabled
    ) with size => 1;

    for LW_CSEC_FALCON_BROM_ENGIDMASK_LWENC_FIELD use
    (
        Lw_Csec_Falcon_Brom_Engidmask_Lwenc_Disabled => 0,
        Lw_Csec_Falcon_Brom_Engidmask_Lwenc_Enabled => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_BROM_ENGIDMASK_GPCCS_FIELD is
    (
        Lw_Csec_Falcon_Brom_Engidmask_Gpccs_Disabled,
        Lw_Csec_Falcon_Brom_Engidmask_Gpccs_Enabled
    ) with size => 1;

    for LW_CSEC_FALCON_BROM_ENGIDMASK_GPCCS_FIELD use
    (
        Lw_Csec_Falcon_Brom_Engidmask_Gpccs_Disabled => 0,
        Lw_Csec_Falcon_Brom_Engidmask_Gpccs_Enabled => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_BROM_ENGIDMASK_FECS_FIELD is
    (
        Lw_Csec_Falcon_Brom_Engidmask_Fecs_Disabled,
        Lw_Csec_Falcon_Brom_Engidmask_Fecs_Enabled
    ) with size => 1;

    for LW_CSEC_FALCON_BROM_ENGIDMASK_FECS_FIELD use
    (
        Lw_Csec_Falcon_Brom_Engidmask_Fecs_Disabled => 0,
        Lw_Csec_Falcon_Brom_Engidmask_Fecs_Enabled => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_BROM_ENGIDMASK_MINION_FIELD is
    (
        Lw_Csec_Falcon_Brom_Engidmask_Minion_Disabled,
        Lw_Csec_Falcon_Brom_Engidmask_Minion_Enabled
    ) with size => 1;

    for LW_CSEC_FALCON_BROM_ENGIDMASK_MINION_FIELD use
    (
        Lw_Csec_Falcon_Brom_Engidmask_Minion_Disabled => 0,
        Lw_Csec_Falcon_Brom_Engidmask_Minion_Enabled => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_BROM_ENGIDMASK_XUSB_FIELD is
    (
        Lw_Csec_Falcon_Brom_Engidmask_Xusb_Disabled,
        Lw_Csec_Falcon_Brom_Engidmask_Xusb_Enabled
    ) with size => 1;

    for LW_CSEC_FALCON_BROM_ENGIDMASK_XUSB_FIELD use
    (
        Lw_Csec_Falcon_Brom_Engidmask_Xusb_Disabled => 0,
        Lw_Csec_Falcon_Brom_Engidmask_Xusb_Enabled => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_BROM_ENGIDMASK_GSP_FIELD is
    (
        Lw_Csec_Falcon_Brom_Engidmask_Gsp_Disabled,
        Lw_Csec_Falcon_Brom_Engidmask_Gsp_Enabled
    ) with size => 1;

    for LW_CSEC_FALCON_BROM_ENGIDMASK_GSP_FIELD use
    (
        Lw_Csec_Falcon_Brom_Engidmask_Gsp_Disabled => 0,
        Lw_Csec_Falcon_Brom_Engidmask_Gsp_Enabled => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_BROM_ENGIDMASK_LWJPG_FIELD is
    (
        Lw_Csec_Falcon_Brom_Engidmask_Lwjpg_Disabled,
        Lw_Csec_Falcon_Brom_Engidmask_Lwjpg_Enabled
    ) with size => 1;

    for LW_CSEC_FALCON_BROM_ENGIDMASK_LWJPG_FIELD use
    (
        Lw_Csec_Falcon_Brom_Engidmask_Lwjpg_Disabled => 0,
        Lw_Csec_Falcon_Brom_Engidmask_Lwjpg_Enabled => 1
    );

---------- Record Declaration ----------

    type LW_CSEC_FALCON_BROM_ENGIDMASK_REGISTER is
    record
        F_Sec    : LW_CSEC_FALCON_BROM_ENGIDMASK_SEC_FIELD;
        F_Dpu    : LW_CSEC_FALCON_BROM_ENGIDMASK_DPU_FIELD;
        F_Lwdec    : LW_CSEC_FALCON_BROM_ENGIDMASK_LWDEC_FIELD;
        F_Pmu    : LW_CSEC_FALCON_BROM_ENGIDMASK_PMU_FIELD;
        F_Fbfalcon    : LW_CSEC_FALCON_BROM_ENGIDMASK_FBFALCON_FIELD;
        F_Lwenc    : LW_CSEC_FALCON_BROM_ENGIDMASK_LWENC_FIELD;
        F_Gpccs    : LW_CSEC_FALCON_BROM_ENGIDMASK_GPCCS_FIELD;
        F_Fecs    : LW_CSEC_FALCON_BROM_ENGIDMASK_FECS_FIELD;
        F_Minion    : LW_CSEC_FALCON_BROM_ENGIDMASK_MINION_FIELD;
        F_Xusb    : LW_CSEC_FALCON_BROM_ENGIDMASK_XUSB_FIELD;
        F_Gsp    : LW_CSEC_FALCON_BROM_ENGIDMASK_GSP_FIELD;
        F_Lwjpg    : LW_CSEC_FALCON_BROM_ENGIDMASK_LWJPG_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_BROM_ENGIDMASK_REGISTER use
    record
        F_Sec at 0 range 0 .. 0;
        F_Dpu at 0 range 1 .. 1;
        F_Lwdec at 0 range 2 .. 2;
        F_Pmu at 0 range 3 .. 3;
        F_Fbfalcon at 0 range 4 .. 4;
        F_Lwenc at 0 range 5 .. 5;
        F_Gpccs at 0 range 6 .. 6;
        F_Fecs at 0 range 7 .. 7;
        F_Minion at 0 range 8 .. 8;
        F_Xusb at 0 range 9 .. 9;
        F_Gsp at 0 range 10 .. 10;
        F_Lwjpg at 0 range 11 .. 11;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Brom_Paraaddr    : constant CSB_ADDR := 16#0004_8400#;

    LW_CSEC_FALCON_BROM_PARAADDR_SIZE_1 : constant := 1;
    subtype LW_CSEC_FALCON_BROM_PARAADDR_INDEX is LwU32 range 0 .. LW_CSEC_FALCON_BROM_PARAADDR_SIZE_1-1;

    function Lw_Csec_Falcon_Brom_Paraaddr_Addr( Index : LW_CSEC_FALCON_BROM_PARAADDR_INDEX )
    return CSB_ADDR is
        (CSB_ADDR(LwU32(Lw_Csec_Falcon_Brom_Paraaddr) + (LwU32(Index)*256)))
    with Inline_Always,
    Global => null,
    Depends => ( Lw_Csec_Falcon_Brom_Paraaddr_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_BROM_PARAADDR_VAL_FIELD is new LwU32;
    Lw_Csec_Falcon_Brom_Paraaddr_Val_Deft :
        constant LW_CSEC_FALCON_BROM_PARAADDR_VAL_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_BROM_PARAADDR_REGISTER is
    record
        F_Val    : LW_CSEC_FALCON_BROM_PARAADDR_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_BROM_PARAADDR_REGISTER use
    record
        F_Val at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Scp_Ctl1    : constant CSB_ADDR := 16#0001_0100#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_SCP_CTL1_SEQ_CLEAR_FIELD is
    (
        Lw_Csec_Scp_Ctl1_Seq_Clear_Idle,
        Lw_Csec_Scp_Ctl1_Seq_Clear_Pending
    ) with size => 1;

    for LW_CSEC_SCP_CTL1_SEQ_CLEAR_FIELD use
    (
        Lw_Csec_Scp_Ctl1_Seq_Clear_Idle => 0,
        Lw_Csec_Scp_Ctl1_Seq_Clear_Pending => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_SCP_CTL1_PIPE_RESET_FIELD is
    (
        Lw_Csec_Scp_Ctl1_Pipe_Reset_Idle,
        Lw_Csec_Scp_Ctl1_Pipe_Reset_Pending
    ) with size => 1;

    for LW_CSEC_SCP_CTL1_PIPE_RESET_FIELD use
    (
        Lw_Csec_Scp_Ctl1_Pipe_Reset_Idle => 0,
        Lw_Csec_Scp_Ctl1_Pipe_Reset_Pending => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_SCP_CTL1_RNG_FAKE_MODE_FIELD is
    (
        Lw_Csec_Scp_Ctl1_Rng_Fake_Mode_Disabled,
        Lw_Csec_Scp_Ctl1_Rng_Fake_Mode_Enabled
    ) with size => 1;

    for LW_CSEC_SCP_CTL1_RNG_FAKE_MODE_FIELD use
    (
        Lw_Csec_Scp_Ctl1_Rng_Fake_Mode_Disabled => 0,
        Lw_Csec_Scp_Ctl1_Rng_Fake_Mode_Enabled => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_SCP_CTL1_RNG_EN_FIELD is
    (
        Lw_Csec_Scp_Ctl1_Rng_En_Disabled,
        Lw_Csec_Scp_Ctl1_Rng_En_Enabled
    ) with size => 1;

    for LW_CSEC_SCP_CTL1_RNG_EN_FIELD use
    (
        Lw_Csec_Scp_Ctl1_Rng_En_Disabled => 0,
        Lw_Csec_Scp_Ctl1_Rng_En_Enabled => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_SCP_CTL1_SF_FETCH_AS_ZERO_FIELD is
    (
        Lw_Csec_Scp_Ctl1_Sf_Fetch_As_Zero_Disabled,
        Lw_Csec_Scp_Ctl1_Sf_Fetch_As_Zero_Enabled
    ) with size => 1;

    for LW_CSEC_SCP_CTL1_SF_FETCH_AS_ZERO_FIELD use
    (
        Lw_Csec_Scp_Ctl1_Sf_Fetch_As_Zero_Disabled => 0,
        Lw_Csec_Scp_Ctl1_Sf_Fetch_As_Zero_Enabled => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_SCP_CTL1_SF_FETCH_BYPASS_FIELD is
    (
        Lw_Csec_Scp_Ctl1_Sf_Fetch_Bypass_Disabled,
        Lw_Csec_Scp_Ctl1_Sf_Fetch_Bypass_Enabled
    ) with size => 1;

    for LW_CSEC_SCP_CTL1_SF_FETCH_BYPASS_FIELD use
    (
        Lw_Csec_Scp_Ctl1_Sf_Fetch_Bypass_Disabled => 0,
        Lw_Csec_Scp_Ctl1_Sf_Fetch_Bypass_Enabled => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_SCP_CTL1_SF_PUSH_BYPASS_FIELD is
    (
        Lw_Csec_Scp_Ctl1_Sf_Push_Bypass_Disabled,
        Lw_Csec_Scp_Ctl1_Sf_Push_Bypass_Enabled
    ) with size => 1;

    for LW_CSEC_SCP_CTL1_SF_PUSH_BYPASS_FIELD use
    (
        Lw_Csec_Scp_Ctl1_Sf_Push_Bypass_Disabled => 0,
        Lw_Csec_Scp_Ctl1_Sf_Push_Bypass_Enabled => 1
    );

---------- Record Declaration ----------

    type LW_CSEC_SCP_CTL1_REGISTER is
    record
        F_Seq_Clear    : LW_CSEC_SCP_CTL1_SEQ_CLEAR_FIELD;
        F_Pipe_Reset    : LW_CSEC_SCP_CTL1_PIPE_RESET_FIELD;
        F_Rng_Fake_Mode    : LW_CSEC_SCP_CTL1_RNG_FAKE_MODE_FIELD;
        F_Rng_En    : LW_CSEC_SCP_CTL1_RNG_EN_FIELD;
        F_Sf_Fetch_As_Zero    : LW_CSEC_SCP_CTL1_SF_FETCH_AS_ZERO_FIELD;
        F_Sf_Fetch_Bypass    : LW_CSEC_SCP_CTL1_SF_FETCH_BYPASS_FIELD;
        F_Sf_Push_Bypass    : LW_CSEC_SCP_CTL1_SF_PUSH_BYPASS_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_SCP_CTL1_REGISTER use
    record
        F_Seq_Clear at 0 range 0 .. 0;
        F_Pipe_Reset at 0 range 8 .. 8;
        F_Rng_Fake_Mode at 0 range 11 .. 11;
        F_Rng_En at 0 range 12 .. 12;
        F_Sf_Fetch_As_Zero at 0 range 16 .. 16;
        F_Sf_Fetch_Bypass at 0 range 20 .. 20;
        F_Sf_Push_Bypass at 0 range 24 .. 24;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Scp_Ctl_Stat    : constant CSB_ADDR := 16#0001_0200#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_SCP_CTL_STAT_SBOOT_FIELD is
    (
        Lw_Csec_Scp_Ctl_Stat_Sboot_False,
        Lw_Csec_Scp_Ctl_Stat_Sboot_True
    ) with size => 1;

    for LW_CSEC_SCP_CTL_STAT_SBOOT_FIELD use
    (
        Lw_Csec_Scp_Ctl_Stat_Sboot_False => 0,
        Lw_Csec_Scp_Ctl_Stat_Sboot_True => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_SCP_CTL_STAT_HSMODE_FIELD is
    (
        Lw_Csec_Scp_Ctl_Stat_Hsmode_False,
        Lw_Csec_Scp_Ctl_Stat_Hsmode_True
    ) with size => 1;

    for LW_CSEC_SCP_CTL_STAT_HSMODE_FIELD use
    (
        Lw_Csec_Scp_Ctl_Stat_Hsmode_False => 0,
        Lw_Csec_Scp_Ctl_Stat_Hsmode_True => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_SCP_CTL_STAT_AES_SCC_DIS_FIELD is
    (
        Lw_Csec_Scp_Ctl_Stat_Aes_Scc_Dis_False,
        Lw_Csec_Scp_Ctl_Stat_Aes_Scc_Dis_True
    ) with size => 1;

    for LW_CSEC_SCP_CTL_STAT_AES_SCC_DIS_FIELD use
    (
        Lw_Csec_Scp_Ctl_Stat_Aes_Scc_Dis_False => 0,
        Lw_Csec_Scp_Ctl_Stat_Aes_Scc_Dis_True => 1
    );

---------- Record Field Type Declaration ----------

    type LW_CSEC_SCP_CTL_STAT_DEBUG_MODE_FIELD is new LwU1;
    Lw_Csec_Scp_Ctl_Stat_Debug_Mode_Disabled :
        constant LW_CSEC_SCP_CTL_STAT_DEBUG_MODE_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_CSEC_SCP_CTL_STAT_REGISTER is
    record
        F_Sboot    : LW_CSEC_SCP_CTL_STAT_SBOOT_FIELD;
        F_Hsmode    : LW_CSEC_SCP_CTL_STAT_HSMODE_FIELD;
        F_Aes_Scc_Dis    : LW_CSEC_SCP_CTL_STAT_AES_SCC_DIS_FIELD;
        F_Debug_Mode    : LW_CSEC_SCP_CTL_STAT_DEBUG_MODE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_SCP_CTL_STAT_REGISTER use
    record
        F_Sboot at 0 range 0 .. 0;
        F_Hsmode at 0 range 1 .. 1;
        F_Aes_Scc_Dis at 0 range 2 .. 2;
        F_Debug_Mode at 0 range 20 .. 20;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Scp_Rndctl0    : constant CSB_ADDR := 16#0001_4000#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_SCP_RNDCTL0_HOLDOFF_INIT_LOWER_FIELD is new LwU32;
    Lw_Csec_Scp_Rndctl0_Holdoff_Init_Lower_Init :
        constant LW_CSEC_SCP_RNDCTL0_HOLDOFF_INIT_LOWER_FIELD
        := 16#01C9_C380#;

---------- Record Declaration ----------

    type LW_CSEC_SCP_RNDCTL0_REGISTER is
    record
        F_Holdoff_Init_Lower    : LW_CSEC_SCP_RNDCTL0_HOLDOFF_INIT_LOWER_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_SCP_RNDCTL0_REGISTER use
    record
        F_Holdoff_Init_Lower at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Scp_Rndctl1    : constant CSB_ADDR := 16#0001_4100#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_SCP_RNDCTL1_HOLDOFF_INIT_UPPER_FIELD is new LwU16;
    Lw_Csec_Scp_Rndctl1_Holdoff_Init_Upper_Zero :
        constant LW_CSEC_SCP_RNDCTL1_HOLDOFF_INIT_UPPER_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_SCP_RNDCTL1_HOLDOFF_INTRA_MASK_FIELD is new LwU16;
    Lw_Csec_Scp_Rndctl1_Holdoff_Intra_Mask_Init :
        constant LW_CSEC_SCP_RNDCTL1_HOLDOFF_INTRA_MASK_FIELD
        := 16#0000_FFFF#;

---------- Record Declaration ----------

    type LW_CSEC_SCP_RNDCTL1_REGISTER is
    record
        F_Holdoff_Init_Upper    : LW_CSEC_SCP_RNDCTL1_HOLDOFF_INIT_UPPER_FIELD;
        F_Holdoff_Intra_Mask    : LW_CSEC_SCP_RNDCTL1_HOLDOFF_INTRA_MASK_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_SCP_RNDCTL1_REGISTER use
    record
        F_Holdoff_Init_Upper at 0 range 0 .. 15;
        F_Holdoff_Intra_Mask at 0 range 16 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Scp_Rndctl11    : constant CSB_ADDR := 16#0001_4B00#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_SCP_RNDCTL11_AUTOCAL_ENABLE_FIELD is new LwU1;
    Lw_Csec_Scp_Rndctl11_Autocal_Enable_Init :
        constant LW_CSEC_SCP_RNDCTL11_AUTOCAL_ENABLE_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_SCP_RNDCTL11_AUTOCAL_MASTERSLAVE_FIELD is new LwU1;
    Lw_Csec_Scp_Rndctl11_Autocal_Masterslave_Init :
        constant LW_CSEC_SCP_RNDCTL11_AUTOCAL_MASTERSLAVE_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_SCP_RNDCTL11_SYNCH_RAND_A_FIELD is new LwU1;
    Lw_Csec_Scp_Rndctl11_Synch_Rand_A_Init :
        constant LW_CSEC_SCP_RNDCTL11_SYNCH_RAND_A_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_SCP_RNDCTL11_SYNCH_RAND_B_FIELD is new LwU1;
    Lw_Csec_Scp_Rndctl11_Synch_Rand_B_Init :
        constant LW_CSEC_SCP_RNDCTL11_SYNCH_RAND_B_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_CSEC_SCP_RNDCTL11_RAND_SAMPLE_SELECT_A_FIELD is
    (
        Lw_Csec_Scp_Rndctl11_Rand_Sample_Select_A_Osc,
        Lw_Csec_Scp_Rndctl11_Rand_Sample_Select_A_Autocal,
        Lw_Csec_Scp_Rndctl11_Rand_Sample_Select_A_Lfsr,
        Lw_Csec_Scp_Rndctl11_Rand_Sample_Select_A_Zero
    ) with size => 2;

    for LW_CSEC_SCP_RNDCTL11_RAND_SAMPLE_SELECT_A_FIELD use
    (
        Lw_Csec_Scp_Rndctl11_Rand_Sample_Select_A_Osc => 0,
        Lw_Csec_Scp_Rndctl11_Rand_Sample_Select_A_Autocal => 1,
        Lw_Csec_Scp_Rndctl11_Rand_Sample_Select_A_Lfsr => 2,
        Lw_Csec_Scp_Rndctl11_Rand_Sample_Select_A_Zero => 3
    );

---------- Enum Declaration ----------

    type LW_CSEC_SCP_RNDCTL11_RAND_SAMPLE_SELECT_B_FIELD is
    (
        Lw_Csec_Scp_Rndctl11_Rand_Sample_Select_B_Osc,
        Lw_Csec_Scp_Rndctl11_Rand_Sample_Select_B_Autocal,
        Lw_Csec_Scp_Rndctl11_Rand_Sample_Select_B_Lfsr,
        Lw_Csec_Scp_Rndctl11_Rand_Sample_Select_B_Zero
    ) with size => 2;

    for LW_CSEC_SCP_RNDCTL11_RAND_SAMPLE_SELECT_B_FIELD use
    (
        Lw_Csec_Scp_Rndctl11_Rand_Sample_Select_B_Osc => 0,
        Lw_Csec_Scp_Rndctl11_Rand_Sample_Select_B_Autocal => 1,
        Lw_Csec_Scp_Rndctl11_Rand_Sample_Select_B_Lfsr => 2,
        Lw_Csec_Scp_Rndctl11_Rand_Sample_Select_B_Zero => 3
    );

---------- Record Field Type Declaration ----------

    type LW_CSEC_SCP_RNDCTL11_AUTOCAL_STATIC_TAP_A_FIELD is new LwU4;
    Lw_Csec_Scp_Rndctl11_Autocal_Static_Tap_A_Init :
        constant LW_CSEC_SCP_RNDCTL11_AUTOCAL_STATIC_TAP_A_FIELD
        := 16#0000_000F#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_SCP_RNDCTL11_AUTOCAL_STATIC_TAP_B_FIELD is new LwU4;
    Lw_Csec_Scp_Rndctl11_Autocal_Static_Tap_B_Init :
        constant LW_CSEC_SCP_RNDCTL11_AUTOCAL_STATIC_TAP_B_FIELD
        := 16#0000_000F#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_SCP_RNDCTL11_MIN_AUTO_TAP_FIELD is new LwU4;
    Lw_Csec_Scp_Rndctl11_Min_Auto_Tap_Init :
        constant LW_CSEC_SCP_RNDCTL11_MIN_AUTO_TAP_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_SCP_RNDCTL11_AUTOCAL_HOLDOFF_DELAY_FIELD is new LwU4;
    Lw_Csec_Scp_Rndctl11_Autocal_Holdoff_Delay_Init :
        constant LW_CSEC_SCP_RNDCTL11_AUTOCAL_HOLDOFF_DELAY_FIELD
        := 16#0000_0001#;

---------- Record Field Type Declaration ----------

    type LW_CSEC_SCP_RNDCTL11_AUTOCAL_ASYNCH_HOLD_DELAY_FIELD is new LwU7;
    Lw_Csec_Scp_Rndctl11_Autocal_Asynch_Hold_Delay_Init :
        constant LW_CSEC_SCP_RNDCTL11_AUTOCAL_ASYNCH_HOLD_DELAY_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_CSEC_SCP_RNDCTL11_AUTOCAL_SAFE_MODE_FIELD is
    (
        Lw_Csec_Scp_Rndctl11_Autocal_Safe_Mode_Disable,
        Lw_Csec_Scp_Rndctl11_Autocal_Safe_Mode_Enable
    ) with size => 1;

    for LW_CSEC_SCP_RNDCTL11_AUTOCAL_SAFE_MODE_FIELD use
    (
        Lw_Csec_Scp_Rndctl11_Autocal_Safe_Mode_Disable => 0,
        Lw_Csec_Scp_Rndctl11_Autocal_Safe_Mode_Enable => 1
    );

---------- Record Declaration ----------

    type LW_CSEC_SCP_RNDCTL11_REGISTER is
    record
        F_Autocal_Enable    : LW_CSEC_SCP_RNDCTL11_AUTOCAL_ENABLE_FIELD;
        F_Autocal_Masterslave    : LW_CSEC_SCP_RNDCTL11_AUTOCAL_MASTERSLAVE_FIELD;
        F_Synch_Rand_A    : LW_CSEC_SCP_RNDCTL11_SYNCH_RAND_A_FIELD;
        F_Synch_Rand_B    : LW_CSEC_SCP_RNDCTL11_SYNCH_RAND_B_FIELD;
        F_Rand_Sample_Select_A    : LW_CSEC_SCP_RNDCTL11_RAND_SAMPLE_SELECT_A_FIELD;
        F_Rand_Sample_Select_B    : LW_CSEC_SCP_RNDCTL11_RAND_SAMPLE_SELECT_B_FIELD;
        F_Autocal_Static_Tap_A    : LW_CSEC_SCP_RNDCTL11_AUTOCAL_STATIC_TAP_A_FIELD;
        F_Autocal_Static_Tap_B    : LW_CSEC_SCP_RNDCTL11_AUTOCAL_STATIC_TAP_B_FIELD;
        F_Min_Auto_Tap    : LW_CSEC_SCP_RNDCTL11_MIN_AUTO_TAP_FIELD;
        F_Autocal_Holdoff_Delay    : LW_CSEC_SCP_RNDCTL11_AUTOCAL_HOLDOFF_DELAY_FIELD;
        F_Autocal_Asynch_Hold_Delay    : LW_CSEC_SCP_RNDCTL11_AUTOCAL_ASYNCH_HOLD_DELAY_FIELD;
        F_Autocal_Safe_Mode    : LW_CSEC_SCP_RNDCTL11_AUTOCAL_SAFE_MODE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_SCP_RNDCTL11_REGISTER use
    record
        F_Autocal_Enable at 0 range 0 .. 0;
        F_Autocal_Masterslave at 0 range 1 .. 1;
        F_Synch_Rand_A at 0 range 2 .. 2;
        F_Synch_Rand_B at 0 range 3 .. 3;
        F_Rand_Sample_Select_A at 0 range 4 .. 5;
        F_Rand_Sample_Select_B at 0 range 6 .. 7;
        F_Autocal_Static_Tap_A at 0 range 8 .. 11;
        F_Autocal_Static_Tap_B at 0 range 12 .. 15;
        F_Min_Auto_Tap at 0 range 16 .. 19;
        F_Autocal_Holdoff_Delay at 0 range 20 .. 23;
        F_Autocal_Asynch_Hold_Delay at 0 range 24 .. 30;
        F_Autocal_Safe_Mode at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Bar0_Csr    : constant CSB_ADDR := 16#0001_C000#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_BAR0_CSR_CMD_FIELD is
    (
        Lw_Csec_Bar0_Csr_Cmd_Undefined,
        Lw_Csec_Bar0_Csr_Cmd_Read,
        Lw_Csec_Bar0_Csr_Cmd_Write
    ) with size => 2;

    for LW_CSEC_BAR0_CSR_CMD_FIELD use
    (
        Lw_Csec_Bar0_Csr_Cmd_Undefined => 0,
        Lw_Csec_Bar0_Csr_Cmd_Read => 1,
        Lw_Csec_Bar0_Csr_Cmd_Write => 2
    );

---------- Record Field Type Declaration ----------

    type LW_CSEC_BAR0_CSR_BYTE_ENABLE_FIELD is new LwU4;
    Lw_Csec_Bar0_Csr_Byte_Enable_Init :
        constant LW_CSEC_BAR0_CSR_BYTE_ENABLE_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_CSEC_BAR0_CSR_XACT_CHECK_FIELD is
    (
        Lw_Csec_Bar0_Csr_Xact_Check_False,
        Lw_Csec_Bar0_Csr_Xact_Check_True
    ) with size => 1;

    for LW_CSEC_BAR0_CSR_XACT_CHECK_FIELD use
    (
        Lw_Csec_Bar0_Csr_Xact_Check_False => 0,
        Lw_Csec_Bar0_Csr_Xact_Check_True => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_BAR0_CSR_XACT_STALLED_FIELD is
    (
        Lw_Csec_Bar0_Csr_Xact_Stalled_False,
        Lw_Csec_Bar0_Csr_Xact_Stalled_True
    ) with size => 1;

    for LW_CSEC_BAR0_CSR_XACT_STALLED_FIELD use
    (
        Lw_Csec_Bar0_Csr_Xact_Stalled_False => 0,
        Lw_Csec_Bar0_Csr_Xact_Stalled_True => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_BAR0_CSR_XACT_FINISHED_FIELD is
    (
        Lw_Csec_Bar0_Csr_Xact_Finished_False,
        Lw_Csec_Bar0_Csr_Xact_Finished_True
    ) with size => 1;

    for LW_CSEC_BAR0_CSR_XACT_FINISHED_FIELD use
    (
        Lw_Csec_Bar0_Csr_Xact_Finished_False => 0,
        Lw_Csec_Bar0_Csr_Xact_Finished_True => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_BAR0_CSR_STATUS_FIELD is
    (
        Lw_Csec_Bar0_Csr_Status_Idle,
        Lw_Csec_Bar0_Csr_Status_Busy,
        Lw_Csec_Bar0_Csr_Status_Tmout,
        Lw_Csec_Bar0_Csr_Status_Dis,
        Lw_Csec_Bar0_Csr_Status_Err
    ) with size => 3;

    for LW_CSEC_BAR0_CSR_STATUS_FIELD use
    (
        Lw_Csec_Bar0_Csr_Status_Idle => 0,
        Lw_Csec_Bar0_Csr_Status_Busy => 1,
        Lw_Csec_Bar0_Csr_Status_Tmout => 2,
        Lw_Csec_Bar0_Csr_Status_Dis => 3,
        Lw_Csec_Bar0_Csr_Status_Err => 4
    );

---------- Record Field Type Declaration ----------

    type LW_CSEC_BAR0_CSR_LEVEL_FIELD is new LwU2;
---------- Enum Declaration ----------

    type LW_CSEC_BAR0_CSR_WARN_FIELD is
    (
        Lw_Csec_Bar0_Csr_Warn_Clean,
        Lw_Csec_Bar0_Csr_Warn_New_Cmd_When_Wip
    ) with size => 2;

    for LW_CSEC_BAR0_CSR_WARN_FIELD use
    (
        Lw_Csec_Bar0_Csr_Warn_Clean => 0,
        Lw_Csec_Bar0_Csr_Warn_New_Cmd_When_Wip => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_BAR0_CSR_HALTED_FIELD is
    (
        Lw_Csec_Bar0_Csr_Halted_False,
        Lw_Csec_Bar0_Csr_Halted_True
    ) with size => 1;

    for LW_CSEC_BAR0_CSR_HALTED_FIELD use
    (
        Lw_Csec_Bar0_Csr_Halted_False => 0,
        Lw_Csec_Bar0_Csr_Halted_True => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_BAR0_CSR_TRIG_FIELD is
    (
        Lw_Csec_Bar0_Csr_Trig_False,
        Lw_Csec_Bar0_Csr_Trig_True
    ) with size => 1;

    for LW_CSEC_BAR0_CSR_TRIG_FIELD use
    (
        Lw_Csec_Bar0_Csr_Trig_False => 0,
        Lw_Csec_Bar0_Csr_Trig_True => 1
    );

---------- Record Declaration ----------

    type LW_CSEC_BAR0_CSR_REGISTER is
    record
        F_Cmd    : LW_CSEC_BAR0_CSR_CMD_FIELD;
        F_Byte_Enable    : LW_CSEC_BAR0_CSR_BYTE_ENABLE_FIELD;
        F_Xact_Check    : LW_CSEC_BAR0_CSR_XACT_CHECK_FIELD;
        F_Xact_Stalled    : LW_CSEC_BAR0_CSR_XACT_STALLED_FIELD;
        F_Xact_Finished    : LW_CSEC_BAR0_CSR_XACT_FINISHED_FIELD;
        F_Status    : LW_CSEC_BAR0_CSR_STATUS_FIELD;
        F_Level    : LW_CSEC_BAR0_CSR_LEVEL_FIELD;
        F_Warn    : LW_CSEC_BAR0_CSR_WARN_FIELD;
        F_Halted    : LW_CSEC_BAR0_CSR_HALTED_FIELD;
        F_Trig    : LW_CSEC_BAR0_CSR_TRIG_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_BAR0_CSR_REGISTER use
    record
        F_Cmd at 0 range 0 .. 1;
        F_Byte_Enable at 0 range 4 .. 7;
        F_Xact_Check at 0 range 8 .. 8;
        F_Xact_Stalled at 0 range 9 .. 9;
        F_Xact_Finished at 0 range 10 .. 10;
        F_Status at 0 range 12 .. 14;
        F_Level at 0 range 16 .. 17;
        F_Warn at 0 range 20 .. 21;
        F_Halted at 0 range 24 .. 24;
        F_Trig at 0 range 31 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Bar0_Addr    : constant CSB_ADDR := 16#0001_C100#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_BAR0_ADDR_REG_ADDR_FIELD is new LwU32;
    Lw_Csec_Bar0_Addr_Reg_Addr_Init :
        constant LW_CSEC_BAR0_ADDR_REG_ADDR_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_CSEC_BAR0_ADDR_REGISTER is
    record
        F_Reg_Addr    : LW_CSEC_BAR0_ADDR_REG_ADDR_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_BAR0_ADDR_REGISTER use
    record
        F_Reg_Addr at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Bar0_Data    : constant CSB_ADDR := 16#0001_C200#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_BAR0_DATA_RWDATA_FIELD is new LwU32;
    Lw_Csec_Bar0_Data_Rwdata_Init :
        constant LW_CSEC_BAR0_DATA_RWDATA_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_CSEC_BAR0_DATA_REGISTER is
    record
        F_Rwdata    : LW_CSEC_BAR0_DATA_RWDATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_BAR0_DATA_REGISTER use
    record
        F_Rwdata at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Bar0_Tmout    : constant CSB_ADDR := 16#0001_C300#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_BAR0_TMOUT_CYCLES_FIELD is
    (
        Lw_Csec_Bar0_Tmout_Cycles_Init,
        Lw_Csec_Bar0_Tmout_Cycles_Prod
    ) with size => 32;

    for LW_CSEC_BAR0_TMOUT_CYCLES_FIELD use
    (
        Lw_Csec_Bar0_Tmout_Cycles_Init => 0,
        Lw_Csec_Bar0_Tmout_Cycles_Prod => 20000000
    );

---------- Record Declaration ----------

    type LW_CSEC_BAR0_TMOUT_REGISTER is
    record
        F_Cycles    : LW_CSEC_BAR0_TMOUT_CYCLES_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_BAR0_TMOUT_REGISTER use
    record
        F_Cycles at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Mailbox    : constant CSB_ADDR := 16#0002_0100#;

    LW_CSEC_MAILBOX_SIZE_1 : constant := 4;
    subtype LW_CSEC_MAILBOX_INDEX is LwU32 range 0 .. LW_CSEC_MAILBOX_SIZE_1-1;

    function Lw_Csec_Mailbox_Addr( Index : LW_CSEC_MAILBOX_INDEX )
    return CSB_ADDR is
        (CSB_ADDR(LwU32(Lw_Csec_Mailbox) + (LwU32(Index)*256)))
    with Inline_Always,
    Global => null,
    Depends => ( Lw_Csec_Mailbox_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_MAILBOX_DATA_FIELD is new LwU32;
    Lw_Csec_Mailbox_Data_Init :
        constant LW_CSEC_MAILBOX_DATA_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_CSEC_MAILBOX_REGISTER is
    record
        F_Data    : LW_CSEC_MAILBOX_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_MAILBOX_REGISTER use
    record
        F_Data at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Mailbox0    : constant CSB_ADDR := 16#0000_1000#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_MAILBOX0_DATA_FIELD is new LwU32;
    Lw_Csec_Falcon_Mailbox0_Data_Init :
        constant LW_CSEC_FALCON_MAILBOX0_DATA_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_MAILBOX0_REGISTER is
    record
        F_Data    : LW_CSEC_FALCON_MAILBOX0_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_MAILBOX0_REGISTER use
    record
        F_Data at 0 range 0 .. 31;
    end record;

   ------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Cmembase    : constant CSB_ADDR := 16#0000_5800#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_CMEMBASE_VAL_FIELD is new LwU14;
    Lw_Csec_Falcon_Cmembase_Val_Init :
        constant LW_CSEC_FALCON_CMEMBASE_VAL_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_CSEC_FALCON_CMEMBASE_REGISTER is
    record
        F_Val    : LW_CSEC_FALCON_CMEMBASE_VAL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_CMEMBASE_REGISTER use
    record
        F_Val at 0 range 18 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Hsctl    : constant CSB_ADDR := 16#0000_3900#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_CSEC_FALCON_HSCTL_TRACEPC_FIELD is
    (
        Lw_Csec_Falcon_Hsctl_Tracepc_Disable,
        Lw_Csec_Falcon_Hsctl_Tracepc_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_HSCTL_TRACEPC_FIELD use
    (
        Lw_Csec_Falcon_Hsctl_Tracepc_Disable => 0,
        Lw_Csec_Falcon_Hsctl_Tracepc_Enable => 1
    );

---------- Enum Declaration ----------

    type LW_CSEC_FALCON_HSCTL_SP_MIN_FIELD is
    (
        Lw_Csec_Falcon_Hsctl_Sp_Min_Disable,
        Lw_Csec_Falcon_Hsctl_Sp_Min_Enable
    ) with size => 1;

    for LW_CSEC_FALCON_HSCTL_SP_MIN_FIELD use
    (
        Lw_Csec_Falcon_Hsctl_Sp_Min_Disable => 0,
        Lw_Csec_Falcon_Hsctl_Sp_Min_Enable => 1
    );

---------- Record Declaration ----------

    type LW_CSEC_FALCON_HSCTL_REGISTER is
    record
        F_Tracepc    : LW_CSEC_FALCON_HSCTL_TRACEPC_FIELD;
        F_Sp_Min    : LW_CSEC_FALCON_HSCTL_SP_MIN_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_HSCTL_REGISTER use
    record
        F_Tracepc at 0 range 0 .. 0;
        F_Sp_Min at 0 range 1 .. 1;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Csec_Falcon_Hwcfg    : constant CSB_ADDR := 16#0000_4200#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_HWCFG_IMEM_SIZE_FIELD is new LwU9;
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_HWCFG_DMEM_SIZE_FIELD is new LwU9;
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_HWCFG_METHODFIFO_DEPTH_FIELD is new LwU9;
---------- Record Field Type Declaration ----------

    type LW_CSEC_FALCON_HWCFG_DMAQUEUE_DEPTH_FIELD is new LwU5;
---------- Record Declaration ----------

    type LW_CSEC_FALCON_HWCFG_REGISTER is
    record
        F_Imem_Size    : LW_CSEC_FALCON_HWCFG_IMEM_SIZE_FIELD;
        F_Dmem_Size    : LW_CSEC_FALCON_HWCFG_DMEM_SIZE_FIELD;
        F_Methodfifo_Depth    : LW_CSEC_FALCON_HWCFG_METHODFIFO_DEPTH_FIELD;
        F_Dmaqueue_Depth    : LW_CSEC_FALCON_HWCFG_DMAQUEUE_DEPTH_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_CSEC_FALCON_HWCFG_REGISTER use
    record
        F_Imem_Size at 0 range 0 .. 8;
        F_Dmem_Size at 0 range 9 .. 17;
        F_Methodfifo_Depth at 0 range 18 .. 26;
        F_Dmaqueue_Depth at 0 range 27 .. 31;
    end record;

end Dev_Sec_Csb_Ada;
