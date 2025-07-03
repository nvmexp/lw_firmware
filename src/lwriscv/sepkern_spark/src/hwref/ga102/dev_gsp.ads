-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;

--******************************  DEV_GSP  ******************************--

package Dev_Gsp
with SPARK_MODE => On
is
   
---------- Register Range Subtype Declaration ----------
   subtype LW_PGSP_BAR0_ADDR is Bar0_Addr range 16#0011_0000# .. 16#0011_3FFF#;
   
   LW_PGSP_FBIF_TRANSCFG : constant := 16#0011_0600#;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pgsp_Falcon_Lockpmb    : constant LW_PGSP_BAR0_ADDR := 16#0011_0170#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PGSP_FALCON_LOCKPMB_IMEM_FIELD is
    (
        Lw_Pgsp_Falcon_Lockpmb_Imem_Init,
        Lw_Pgsp_Falcon_Lockpmb_Imem_Lock
    ) with size => 1;

    for LW_PGSP_FALCON_LOCKPMB_IMEM_FIELD use
    (
        Lw_Pgsp_Falcon_Lockpmb_Imem_Init => 0,
        Lw_Pgsp_Falcon_Lockpmb_Imem_Lock => 1
    );

    Lw_Pgsp_Falcon_Lockpmb_Imem_Unlock :
        constant LW_PGSP_FALCON_LOCKPMB_IMEM_FIELD
        := Lw_Pgsp_Falcon_Lockpmb_Imem_Init;

---------- Enum Declaration ----------

    type LW_PGSP_FALCON_LOCKPMB_DMEM_FIELD is
    (
        Lw_Pgsp_Falcon_Lockpmb_Dmem_Init,
        Lw_Pgsp_Falcon_Lockpmb_Dmem_Lock
    ) with size => 1;

    for LW_PGSP_FALCON_LOCKPMB_DMEM_FIELD use
    (
        Lw_Pgsp_Falcon_Lockpmb_Dmem_Init => 0,
        Lw_Pgsp_Falcon_Lockpmb_Dmem_Lock => 1
    );

    Lw_Pgsp_Falcon_Lockpmb_Dmem_Unlock :
        constant LW_PGSP_FALCON_LOCKPMB_DMEM_FIELD
        := Lw_Pgsp_Falcon_Lockpmb_Dmem_Init;

---------- Record Declaration ----------

    type LW_PGSP_FALCON_LOCKPMB_REGISTER is
    record
        F_Imem    : LW_PGSP_FALCON_LOCKPMB_IMEM_FIELD;
        F_Dmem    : LW_PGSP_FALCON_LOCKPMB_DMEM_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGSP_FALCON_LOCKPMB_REGISTER use
    record
        F_Imem at 0 range 0 .. 0;
        F_Dmem at 0 range 1 .. 1;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pgsp_Falcon_Debug1    : constant LW_PGSP_BAR0_ADDR := 16#0011_0090#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PGSP_FALCON_DEBUG1_MTHD_DRAIN_TIME_FIELD is new LwU16;
    Lw_Pgsp_Falcon_Debug1_Mthd_Drain_Time_Init :
        constant LW_PGSP_FALCON_DEBUG1_MTHD_DRAIN_TIME_FIELD
        := 16#0000_0040#;

---------- Record Field Type Declaration ----------

    type LW_PGSP_FALCON_DEBUG1_CTXSW_MODE_FIELD is new LwU1;
    Lw_Pgsp_Falcon_Debug1_Ctxsw_Mode_Init :
        constant LW_PGSP_FALCON_DEBUG1_CTXSW_MODE_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_PGSP_FALCON_DEBUG1_TRACE_FORMAT_FIELD is
    (
        Lw_Pgsp_Falcon_Debug1_Trace_Format_Init,
        Lw_Pgsp_Falcon_Debug1_Trace_Format_Compressed
    ) with size => 1;

    for LW_PGSP_FALCON_DEBUG1_TRACE_FORMAT_FIELD use
    (
        Lw_Pgsp_Falcon_Debug1_Trace_Format_Init => 0,
        Lw_Pgsp_Falcon_Debug1_Trace_Format_Compressed => 1
    );

    Lw_Pgsp_Falcon_Debug1_Trace_Format_Uncompressed :
        constant LW_PGSP_FALCON_DEBUG1_TRACE_FORMAT_FIELD
        := Lw_Pgsp_Falcon_Debug1_Trace_Format_Init;

---------- Enum Declaration ----------

    type LW_PGSP_FALCON_DEBUG1_CTXSW_MODE1_FIELD is
    (
        Lw_Pgsp_Falcon_Debug1_Ctxsw_Mode1_Init,
        Lw_Pgsp_Falcon_Debug1_Ctxsw_Mode1_Bypass_Idle_Checks
    ) with size => 1;

    for LW_PGSP_FALCON_DEBUG1_CTXSW_MODE1_FIELD use
    (
        Lw_Pgsp_Falcon_Debug1_Ctxsw_Mode1_Init => 0,
        Lw_Pgsp_Falcon_Debug1_Ctxsw_Mode1_Bypass_Idle_Checks => 1
    );

    Lw_Pgsp_Falcon_Debug1_Ctxsw_Mode1_Default :
        constant LW_PGSP_FALCON_DEBUG1_CTXSW_MODE1_FIELD
        := Lw_Pgsp_Falcon_Debug1_Ctxsw_Mode1_Init;

---------- Record Declaration ----------

    type LW_PGSP_FALCON_DEBUG1_REGISTER is
    record
        F_Mthd_Drain_Time    : LW_PGSP_FALCON_DEBUG1_MTHD_DRAIN_TIME_FIELD;
        F_Ctxsw_Mode    : LW_PGSP_FALCON_DEBUG1_CTXSW_MODE_FIELD;
        F_Trace_Format    : LW_PGSP_FALCON_DEBUG1_TRACE_FORMAT_FIELD;
        F_Ctxsw_Mode1    : LW_PGSP_FALCON_DEBUG1_CTXSW_MODE1_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PGSP_FALCON_DEBUG1_REGISTER use
    record
        F_Mthd_Drain_Time at 0 range 0 .. 15;
        F_Ctxsw_Mode at 0 range 16 .. 16;
        F_Trace_Format at 0 range 17 .. 17;
        F_Ctxsw_Mode1 at 0 range 18 .. 18;
    end record;

   
end Dev_Gsp;
