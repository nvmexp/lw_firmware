-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;

--******************************  DEV_PWR_PRI  ******************************--

package Dev_Pwr_Pri
with SPARK_MODE => On
is


---------- Register Range Subtype Declaration ----------
    subtype LW_PPWR_BAR0_ADDR is Bar0_Addr range 16#0010_A000# .. 16#0010_BFFF#;
------------------------------------------------------------------------------------------------------

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppwr_Falcon_Lockpmb    : constant LW_PPWR_BAR0_ADDR := 16#0010_A170#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPWR_FALCON_LOCKPMB_IMEM_FIELD is
    (
        Lw_Ppwr_Falcon_Lockpmb_Imem_Init,
        Lw_Ppwr_Falcon_Lockpmb_Imem_Lock
    ) with size => 1;

    for LW_PPWR_FALCON_LOCKPMB_IMEM_FIELD use
    (
        Lw_Ppwr_Falcon_Lockpmb_Imem_Init => 0,
        Lw_Ppwr_Falcon_Lockpmb_Imem_Lock => 1
    );

    Lw_Ppwr_Falcon_Lockpmb_Imem_Unlock :
        constant LW_PPWR_FALCON_LOCKPMB_IMEM_FIELD
        := Lw_Ppwr_Falcon_Lockpmb_Imem_Init;

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_LOCKPMB_DMEM_FIELD is
    (
        Lw_Ppwr_Falcon_Lockpmb_Dmem_Init,
        Lw_Ppwr_Falcon_Lockpmb_Dmem_Lock
    ) with size => 1;

    for LW_PPWR_FALCON_LOCKPMB_DMEM_FIELD use
    (
        Lw_Ppwr_Falcon_Lockpmb_Dmem_Init => 0,
        Lw_Ppwr_Falcon_Lockpmb_Dmem_Lock => 1
    );

    Lw_Ppwr_Falcon_Lockpmb_Dmem_Unlock :
        constant LW_PPWR_FALCON_LOCKPMB_DMEM_FIELD
        := Lw_Ppwr_Falcon_Lockpmb_Dmem_Init;

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_LOCKPMB_EXT2MEM_FIELD is
    (
        Lw_Ppwr_Falcon_Lockpmb_Ext2Mem_Init,
        Lw_Ppwr_Falcon_Lockpmb_Ext2Mem_Lock
    ) with size => 1;

    for LW_PPWR_FALCON_LOCKPMB_EXT2MEM_FIELD use
    (
        Lw_Ppwr_Falcon_Lockpmb_Ext2Mem_Init => 0,
        Lw_Ppwr_Falcon_Lockpmb_Ext2Mem_Lock => 1
    );

    Lw_Ppwr_Falcon_Lockpmb_Ext2Mem_Unlock :
        constant LW_PPWR_FALCON_LOCKPMB_EXT2MEM_FIELD
        := Lw_Ppwr_Falcon_Lockpmb_Ext2Mem_Init;

---------- Record Declaration ----------

    type LW_PPWR_FALCON_LOCKPMB_REGISTER is
    record
        F_Imem    : LW_PPWR_FALCON_LOCKPMB_IMEM_FIELD;
        F_Dmem    : LW_PPWR_FALCON_LOCKPMB_DMEM_FIELD;
        F_Ext2Mem    : LW_PPWR_FALCON_LOCKPMB_EXT2MEM_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPWR_FALCON_LOCKPMB_REGISTER use
    record
        F_Imem at 0 range 0 .. 0;
        F_Dmem at 0 range 1 .. 1;
        F_Ext2Mem at 0 range 4 .. 4;
    end record;

------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppwr_Fbif_Transcfg    : constant LW_PPWR_BAR0_ADDR := 16#0010_AE00#;

   LW_PPWR_FBIF_TRANSCFG_SIZE_1 : constant := 8;
   LW_PPWR_FBIF_TRANSCFG_MAX_INDEX : constant := 8;
   subtype LW_PPWR_FBIF_TRANSCFG_INDEX is LwU32 range 0 .. LW_PPWR_FBIF_TRANSCFG_MAX_INDEX;

    function Lw_Ppwr_Fbif_Transcfg_Addr( Index : LW_PPWR_FBIF_TRANSCFG_INDEX )
    return LW_PPWR_BAR0_ADDR is
        (LW_PPWR_BAR0_ADDR(LwU32(Lw_Ppwr_Fbif_Transcfg) + (LwU32(Index)*4)))
    with Inline_Always,
    Global => null,
    Depends => ( Lw_Ppwr_Fbif_Transcfg_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PPWR_FBIF_TRANSCFG_TARGET_FIELD is
    (
        Lw_Ppwr_Fbif_Transcfg_Target_Init,
        Lw_Ppwr_Fbif_Transcfg_Target_Coherent_Sysmem,
        Lw_Ppwr_Fbif_Transcfg_Target_Noncoherent_Sysmem
    ) with size => 2;

    for LW_PPWR_FBIF_TRANSCFG_TARGET_FIELD use
    (
        Lw_Ppwr_Fbif_Transcfg_Target_Init => 0,
        Lw_Ppwr_Fbif_Transcfg_Target_Coherent_Sysmem => 1,
        Lw_Ppwr_Fbif_Transcfg_Target_Noncoherent_Sysmem => 2
    );

    Lw_Ppwr_Fbif_Transcfg_Target_Local_Fb :
        constant LW_PPWR_FBIF_TRANSCFG_TARGET_FIELD
        := Lw_Ppwr_Fbif_Transcfg_Target_Init;

---------- Enum Declaration ----------

    type LW_PPWR_FBIF_TRANSCFG_MEM_TYPE_FIELD is
    (
        Lw_Ppwr_Fbif_Transcfg_Mem_Type_Init,
        Lw_Ppwr_Fbif_Transcfg_Mem_Type_Physical
    ) with size => 1;

    for LW_PPWR_FBIF_TRANSCFG_MEM_TYPE_FIELD use
    (
        Lw_Ppwr_Fbif_Transcfg_Mem_Type_Init => 0,
        Lw_Ppwr_Fbif_Transcfg_Mem_Type_Physical => 1
    );

    Lw_Ppwr_Fbif_Transcfg_Mem_Type_Virtual :
        constant LW_PPWR_FBIF_TRANSCFG_MEM_TYPE_FIELD
        := Lw_Ppwr_Fbif_Transcfg_Mem_Type_Init;

---------- Enum Declaration ----------

    type LW_PPWR_FBIF_TRANSCFG_L2C_WR_FIELD is
    (
        Lw_Ppwr_Fbif_Transcfg_L2C_Wr_L2_Evict_First,
        Lw_Ppwr_Fbif_Transcfg_L2C_Wr_Init,
        Lw_Ppwr_Fbif_Transcfg_L2C_Wr_L2_Evict_Last
    ) with size => 2;

    for LW_PPWR_FBIF_TRANSCFG_L2C_WR_FIELD use
    (
        Lw_Ppwr_Fbif_Transcfg_L2C_Wr_L2_Evict_First => 0,
        Lw_Ppwr_Fbif_Transcfg_L2C_Wr_Init => 1,
        Lw_Ppwr_Fbif_Transcfg_L2C_Wr_L2_Evict_Last => 2
    );

    Lw_Ppwr_Fbif_Transcfg_L2C_Wr_L2_Evict_Normal :
        constant LW_PPWR_FBIF_TRANSCFG_L2C_WR_FIELD
        := Lw_Ppwr_Fbif_Transcfg_L2C_Wr_Init;

---------- Enum Declaration ----------

    type LW_PPWR_FBIF_TRANSCFG_L2C_RD_FIELD is
    (
        Lw_Ppwr_Fbif_Transcfg_L2C_Rd_L2_Evict_First,
        Lw_Ppwr_Fbif_Transcfg_L2C_Rd_Init,
        Lw_Ppwr_Fbif_Transcfg_L2C_Rd_L2_Evict_Last
    ) with size => 2;

    for LW_PPWR_FBIF_TRANSCFG_L2C_RD_FIELD use
    (
        Lw_Ppwr_Fbif_Transcfg_L2C_Rd_L2_Evict_First => 0,
        Lw_Ppwr_Fbif_Transcfg_L2C_Rd_Init => 1,
        Lw_Ppwr_Fbif_Transcfg_L2C_Rd_L2_Evict_Last => 2
    );

    Lw_Ppwr_Fbif_Transcfg_L2C_Rd_L2_Evict_Normal :
        constant LW_PPWR_FBIF_TRANSCFG_L2C_RD_FIELD
        := Lw_Ppwr_Fbif_Transcfg_L2C_Rd_Init;

---------- Enum Declaration ----------

    type LW_PPWR_FBIF_TRANSCFG_WACHK0_FIELD is
    (
        Lw_Ppwr_Fbif_Transcfg_Wachk0_Init,
        Lw_Ppwr_Fbif_Transcfg_Wachk0_Enable
    ) with size => 1;

    for LW_PPWR_FBIF_TRANSCFG_WACHK0_FIELD use
    (
        Lw_Ppwr_Fbif_Transcfg_Wachk0_Init => 0,
        Lw_Ppwr_Fbif_Transcfg_Wachk0_Enable => 1
    );

    Lw_Ppwr_Fbif_Transcfg_Wachk0_Disable :
        constant LW_PPWR_FBIF_TRANSCFG_WACHK0_FIELD
        := Lw_Ppwr_Fbif_Transcfg_Wachk0_Init;

---------- Enum Declaration ----------

    type LW_PPWR_FBIF_TRANSCFG_WACHK1_FIELD is
    (
        Lw_Ppwr_Fbif_Transcfg_Wachk1_Init,
        Lw_Ppwr_Fbif_Transcfg_Wachk1_Enable
    ) with size => 1;

    for LW_PPWR_FBIF_TRANSCFG_WACHK1_FIELD use
    (
        Lw_Ppwr_Fbif_Transcfg_Wachk1_Init => 0,
        Lw_Ppwr_Fbif_Transcfg_Wachk1_Enable => 1
    );

    Lw_Ppwr_Fbif_Transcfg_Wachk1_Disable :
        constant LW_PPWR_FBIF_TRANSCFG_WACHK1_FIELD
        := Lw_Ppwr_Fbif_Transcfg_Wachk1_Init;

---------- Enum Declaration ----------

    type LW_PPWR_FBIF_TRANSCFG_RACHK0_FIELD is
    (
        Lw_Ppwr_Fbif_Transcfg_Rachk0_Init,
        Lw_Ppwr_Fbif_Transcfg_Rachk0_Enable
    ) with size => 1;

    for LW_PPWR_FBIF_TRANSCFG_RACHK0_FIELD use
    (
        Lw_Ppwr_Fbif_Transcfg_Rachk0_Init => 0,
        Lw_Ppwr_Fbif_Transcfg_Rachk0_Enable => 1
    );

    Lw_Ppwr_Fbif_Transcfg_Rachk0_Disable :
        constant LW_PPWR_FBIF_TRANSCFG_RACHK0_FIELD
        := Lw_Ppwr_Fbif_Transcfg_Rachk0_Init;

---------- Enum Declaration ----------

    type LW_PPWR_FBIF_TRANSCFG_RACHK1_FIELD is
    (
        Lw_Ppwr_Fbif_Transcfg_Rachk1_Init,
        Lw_Ppwr_Fbif_Transcfg_Rachk1_Enable
    ) with size => 1;

    for LW_PPWR_FBIF_TRANSCFG_RACHK1_FIELD use
    (
        Lw_Ppwr_Fbif_Transcfg_Rachk1_Init => 0,
        Lw_Ppwr_Fbif_Transcfg_Rachk1_Enable => 1
    );

    Lw_Ppwr_Fbif_Transcfg_Rachk1_Disable :
        constant LW_PPWR_FBIF_TRANSCFG_RACHK1_FIELD
        := Lw_Ppwr_Fbif_Transcfg_Rachk1_Init;

---------- Record Declaration ----------

    type LW_PPWR_FBIF_TRANSCFG_REGISTER is
    record
        F_Target    : LW_PPWR_FBIF_TRANSCFG_TARGET_FIELD;
        F_Mem_Type    : LW_PPWR_FBIF_TRANSCFG_MEM_TYPE_FIELD;
        F_L2C_Wr    : LW_PPWR_FBIF_TRANSCFG_L2C_WR_FIELD;
        F_L2C_Rd    : LW_PPWR_FBIF_TRANSCFG_L2C_RD_FIELD;
        F_Wachk0    : LW_PPWR_FBIF_TRANSCFG_WACHK0_FIELD;
        F_Wachk1    : LW_PPWR_FBIF_TRANSCFG_WACHK1_FIELD;
        F_Rachk0    : LW_PPWR_FBIF_TRANSCFG_RACHK0_FIELD;
        F_Rachk1    : LW_PPWR_FBIF_TRANSCFG_RACHK1_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPWR_FBIF_TRANSCFG_REGISTER use
    record
        F_Target at 0 range 0 .. 1;
        F_Mem_Type at 0 range 2 .. 2;
        F_L2C_Wr at 0 range 4 .. 5;
        F_L2C_Rd at 0 range 8 .. 9;
        F_Wachk0 at 0 range 12 .. 12;
        F_Wachk1 at 0 range 13 .. 13;
        F_Rachk0 at 0 range 14 .. 14;
        F_Rachk1 at 0 range 15 .. 15;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Ppwr_Falcon_Debug1    : constant LW_PPWR_BAR0_ADDR := 16#0010_A090#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PPWR_FALCON_DEBUG1_MTHD_DRAIN_TIME_FIELD is new LwU16;
    Lw_Ppwr_Falcon_Debug1_Mthd_Drain_Time_Init :
        constant LW_PPWR_FALCON_DEBUG1_MTHD_DRAIN_TIME_FIELD
        := 16#0000_0040#;

---------- Record Field Type Declaration ----------

    type LW_PPWR_FALCON_DEBUG1_CTXSW_MODE_FIELD is new LwU1;
    Lw_Ppwr_Falcon_Debug1_Ctxsw_Mode_Init :
        constant LW_PPWR_FALCON_DEBUG1_CTXSW_MODE_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_DEBUG1_TRACE_FORMAT_FIELD is
    (
        Lw_Ppwr_Falcon_Debug1_Trace_Format_Init,
        Lw_Ppwr_Falcon_Debug1_Trace_Format_Compressed
    ) with size => 1;

    for LW_PPWR_FALCON_DEBUG1_TRACE_FORMAT_FIELD use
    (
        Lw_Ppwr_Falcon_Debug1_Trace_Format_Init => 0,
        Lw_Ppwr_Falcon_Debug1_Trace_Format_Compressed => 1
    );

    Lw_Ppwr_Falcon_Debug1_Trace_Format_Uncompressed :
        constant LW_PPWR_FALCON_DEBUG1_TRACE_FORMAT_FIELD
        := Lw_Ppwr_Falcon_Debug1_Trace_Format_Init;

---------- Enum Declaration ----------

    type LW_PPWR_FALCON_DEBUG1_CTXSW_MODE1_FIELD is
    (
        Lw_Ppwr_Falcon_Debug1_Ctxsw_Mode1_Init,
        Lw_Ppwr_Falcon_Debug1_Ctxsw_Mode1_Bypass_Idle_Checks
    ) with size => 1;

    for LW_PPWR_FALCON_DEBUG1_CTXSW_MODE1_FIELD use
    (
        Lw_Ppwr_Falcon_Debug1_Ctxsw_Mode1_Init => 0,
        Lw_Ppwr_Falcon_Debug1_Ctxsw_Mode1_Bypass_Idle_Checks => 1
    );

    Lw_Ppwr_Falcon_Debug1_Ctxsw_Mode1_Default :
        constant LW_PPWR_FALCON_DEBUG1_CTXSW_MODE1_FIELD
        := Lw_Ppwr_Falcon_Debug1_Ctxsw_Mode1_Init;

---------- Record Declaration ----------

    type LW_PPWR_FALCON_DEBUG1_REGISTER is
    record
        F_Mthd_Drain_Time    : LW_PPWR_FALCON_DEBUG1_MTHD_DRAIN_TIME_FIELD;
        F_Ctxsw_Mode    : LW_PPWR_FALCON_DEBUG1_CTXSW_MODE_FIELD;
        F_Trace_Format    : LW_PPWR_FALCON_DEBUG1_TRACE_FORMAT_FIELD;
        F_Ctxsw_Mode1    : LW_PPWR_FALCON_DEBUG1_CTXSW_MODE1_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PPWR_FALCON_DEBUG1_REGISTER use
    record
        F_Mthd_Drain_Time at 0 range 0 .. 15;
        F_Ctxsw_Mode at 0 range 16 .. 16;
        F_Trace_Format at 0 range 17 .. 17;
        F_Ctxsw_Mode1 at 0 range 18 .. 18;
    end record;
------------------------------------------------------------------------------------------------------


------------------------------------------------------------------------------------------------------
end Dev_Pwr_Pri;
