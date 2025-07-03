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

--******************************  DEV_SE_SEB  ******************************--

package Dev_Se_Seb_Ada
with SPARK_MODE => On
is


   ---------- Register Range Subtype Declaration ----------
   subtype LW_SSE_BAR0_ADDR is BAR0_ADDR range 16#0000_0000# .. 16#0000_FFFF#;
   ------------------------------------------------------------------------------------------------------
   ---------- Register Declaration ----------

   Lw_Sse_Se_Ctrl_Pka_Mutex    : constant LW_SSE_BAR0_ADDR := 16#0000_0114#;

   ------------------------------------------------------------------------------------------------------
   ---------- Record Field Type Declaration ----------

   type LW_SSE_SE_CTRL_PKA_MUTEX_SE_LOCK_FIELD is new LwU1;
   Lw_Sse_Se_Ctrl_Pka_Mutex_Se_Lock_Release :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_SE_LOCK_FIELD
     := 16#0000_0001#;

   ---------- Enum Declaration ----------

   type LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD is
     (
      Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Scrubbing,
      Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Pmu,
      Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Lwdec,
      Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Sec,
      Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Dpu,
      Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Priv
     ) with size => 8;

   for LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD use
     (
      Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Scrubbing => 0,
      Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Pmu => 1,
      Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Lwdec => 2,
      Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Sec => 3,
      Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Dpu => 4,
      Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Priv => 5
     );

   Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Level_0 :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD
     := Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Scrubbing;

   Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Level_Nonselwre :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD
     := Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Scrubbing;

   Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Tzselwre :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD
     := Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Pmu;

   Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Level_1 :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD
     := Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Pmu;

   Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Level_Tzselwre :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD
     := Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Pmu;

   Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Lwselwre :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD
     := Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Lwdec;

   Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Level_2 :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD
     := Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Lwdec;

   Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Level_Lwselwre :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD
     := Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Lwdec;

   Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Lwtzselwre :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD
     := Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Sec;

   Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Oemselwre :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD
     := Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Sec;

   Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Level_3 :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD
     := Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Sec;

   Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Level_Lwtzselwre :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD
     := Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Sec;

   Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Level_Oemselwre :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD
     := Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Sec;

   Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Nonselwre :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD
     := Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Master_Dpu;

   ---------- Record Declaration ----------

   type LW_SSE_SE_CTRL_PKA_MUTEX_REGISTER is
      record
         F_Se_Lock    : LW_SSE_SE_CTRL_PKA_MUTEX_SE_LOCK_FIELD;
         F_Rc_Reason    : LW_SSE_SE_CTRL_PKA_MUTEX_RC_REASON_FIELD;
      end record
     with  Size => 32, Object_size => 32, Alignment => 4;

   for LW_SSE_SE_CTRL_PKA_MUTEX_REGISTER use
      record
         F_Se_Lock at 0 range 0 .. 0;
         F_Rc_Reason at 0 range 24 .. 31;
      end record;

   ------------------------------------------------------------------------------------------------------
   ---------- Register Declaration ----------

   Lw_Sse_Se_Ctrl_Pka_Mutex_Tmout_Action    : constant LW_SSE_BAR0_ADDR := 16#0000_0128#;

   ------------------------------------------------------------------------------------------------------
   ---------- Enum Declaration ----------

   type LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION_INTERRUPT_FIELD is
     (
      Lw_Sse_Se_Ctrl_Pka_Mutex_Tmout_Action_Interrupt_Disable,
      Lw_Sse_Se_Ctrl_Pka_Mutex_Tmout_Action_Interrupt_Enable
     ) with size => 1;

   for LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION_INTERRUPT_FIELD use
     (
      Lw_Sse_Se_Ctrl_Pka_Mutex_Tmout_Action_Interrupt_Disable => 0,
      Lw_Sse_Se_Ctrl_Pka_Mutex_Tmout_Action_Interrupt_Enable => 1
     );

   ---------- Record Field Type Declaration ----------

   type LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION_RELEASE_MUTEX_FIELD is new LwU1;
   Lw_Sse_Se_Ctrl_Pka_Mutex_Tmout_Action_Release_Mutex_Enable :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION_RELEASE_MUTEX_FIELD
     := 16#0000_0001#;

   ---------- Record Field Type Declaration ----------

   type LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION_RESET_PKA_FIELD is new LwU1;
   Lw_Sse_Se_Ctrl_Pka_Mutex_Tmout_Action_Reset_Pka_Enable :
   constant LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION_RESET_PKA_FIELD
     := 16#0000_0001#;

   ---------- Enum Declaration ----------

   type LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION_STATUS_FIELD is
     (
      Lw_Sse_Se_Ctrl_Pka_Mutex_Tmout_Action_Status_False,
      Lw_Sse_Se_Ctrl_Pka_Mutex_Tmout_Action_Status_True
     ) with size => 1;

   for LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION_STATUS_FIELD use
     (
      Lw_Sse_Se_Ctrl_Pka_Mutex_Tmout_Action_Status_False => 0,
      Lw_Sse_Se_Ctrl_Pka_Mutex_Tmout_Action_Status_True => 1
     );

   ---------- Record Declaration ----------

   type LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION_REGISTER is
      record
         F_Interrupt    : LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION_INTERRUPT_FIELD;
         F_Release_Mutex    : LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION_RELEASE_MUTEX_FIELD;
         F_Reset_Pka    : LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION_RESET_PKA_FIELD;
         F_Status    : LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION_STATUS_FIELD;
      end record
     with  Size => 32, Object_size => 32, Alignment => 4;

   for LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION_REGISTER use
      record
         F_Interrupt at 0 range 0 .. 0;
         F_Release_Mutex at 0 range 1 .. 1;
         F_Reset_Pka at 0 range 2 .. 2;
         F_Status at 0 range 3 .. 3;
      end record;

   ------------------------------------------------------------------------------------------------------
   ---------- Register Declaration ----------

   Lw_Sse_Se_Ctrl_Se_Mutex_Tmout_Dftval    : constant LW_SSE_BAR0_ADDR := 16#0000_0144#;

   ------------------------------------------------------------------------------------------------------
   ---------- Record Field Type Declaration ----------

   type LW_SSE_SE_CTRL_SE_MUTEX_TMOUT_DFTVAL_TMOUT_FIELD is new LwU32;
   Lw_Sse_Se_Ctrl_Se_Mutex_Tmout_Dftval_Tmout_Max :
   constant LW_SSE_SE_CTRL_SE_MUTEX_TMOUT_DFTVAL_TMOUT_FIELD
     := 16#FFFF_FFFF#;

   ---------- Record Declaration ----------

   type LW_SSE_SE_CTRL_SE_MUTEX_TMOUT_DFTVAL_REGISTER is
      record
         F_Tmout    : LW_SSE_SE_CTRL_SE_MUTEX_TMOUT_DFTVAL_TMOUT_FIELD;
      end record
     with  Size => 32, Object_size => 32, Alignment => 4;

   for LW_SSE_SE_CTRL_SE_MUTEX_TMOUT_DFTVAL_REGISTER use
      record
         F_Tmout at 0 range 0 .. 31;
      end record;
------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Sse_Se_Common_Mutex_Id    : constant LW_SSE_BAR0_ADDR := 16#0000_8E04#;

    LW_SSE_SE_COMMON_MUTEX_ID_SIZE_1 : constant := 3;
    subtype LW_SSE_SE_COMMON_MUTEX_ID_INDEX is LwU32 range 0 .. LW_SSE_SE_COMMON_MUTEX_ID_SIZE_1-1;

    function Lw_Sse_Se_Common_Mutex_Id_Addr( Index : LW_SSE_SE_COMMON_MUTEX_ID_INDEX )
    return LW_SSE_BAR0_ADDR is
        (LW_SSE_BAR0_ADDR(LwU32(Lw_Sse_Se_Common_Mutex_Id) + (LwU32(Index)*80)))
    with Inline_Always,
    Global => null,
    Depends => ( Lw_Sse_Se_Common_Mutex_Id_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_SSE_SE_COMMON_MUTEX_ID_VALUE_FIELD is
    (
        Lw_Sse_Se_Common_Mutex_Id_Value_Init,
        Lw_Sse_Se_Common_Mutex_Id_Value_Not_Avail
    ) with size => 8;

    for LW_SSE_SE_COMMON_MUTEX_ID_VALUE_FIELD use
    (
        Lw_Sse_Se_Common_Mutex_Id_Value_Init => 0,
        Lw_Sse_Se_Common_Mutex_Id_Value_Not_Avail => 255
    );

---------- Record Declaration ----------

    type LW_SSE_SE_COMMON_MUTEX_ID_REGISTER is
    record
        F_Value    : LW_SSE_SE_COMMON_MUTEX_ID_VALUE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_SSE_SE_COMMON_MUTEX_ID_REGISTER use
    record
        F_Value at 0 range 0 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Sse_Se_Common_Mutex_Id_Release    : constant LW_SSE_BAR0_ADDR := 16#0000_8E08#;

    LW_SSE_SE_COMMON_MUTEX_ID_RELEASE_SIZE_1 : constant := 3;
    subtype LW_SSE_SE_COMMON_MUTEX_ID_RELEASE_INDEX is LwU32 range 0 .. LW_SSE_SE_COMMON_MUTEX_ID_RELEASE_SIZE_1-1;

    function Lw_Sse_Se_Common_Mutex_Id_Release_Addr( Index : LW_SSE_SE_COMMON_MUTEX_ID_RELEASE_INDEX )
    return LW_SSE_BAR0_ADDR is
        (LW_SSE_BAR0_ADDR(LwU32(Lw_Sse_Se_Common_Mutex_Id_Release) + (LwU32(Index)*80)))
    with Inline_Always,
    Global => null,
    Depends => ( Lw_Sse_Se_Common_Mutex_Id_Release_Addr'Result => Index );
------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_SSE_SE_COMMON_MUTEX_ID_RELEASE_VALUE_FIELD is new LwU8;
    Lw_Sse_Se_Common_Mutex_Id_Release_Value_Init :
        constant LW_SSE_SE_COMMON_MUTEX_ID_RELEASE_VALUE_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_SSE_SE_COMMON_MUTEX_ID_RELEASE_REGISTER is
    record
        F_Value    : LW_SSE_SE_COMMON_MUTEX_ID_RELEASE_VALUE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_SSE_SE_COMMON_MUTEX_ID_RELEASE_REGISTER use
    record
        F_Value at 0 range 0 .. 7;
    end record;

   ------------------------------------------------------------------------------------------------------
     ---------- Register Declaration ----------

    Lw_Sse_Se_Common_Mutex_Mutex    : constant LW_SSE_BAR0_ADDR := 16#0000_8E0C#;

    LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_1 : constant := 3;
    subtype LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_1_INDEX is LwU32 range 0 .. LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_1-1;

    LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_2 : constant := 16;
    subtype LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_2_INDEX is LwU32 range 0 .. LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_2-1;

    function Lw_Sse_Se_Common_Mutex_Mutex_Addr( Index_I : LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_1_INDEX;
                                                Index_J : LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_2_INDEX)
    return LW_SSE_BAR0_ADDR is
        (LW_SSE_BAR0_ADDR(LwU32(Lw_Sse_Se_Common_Mutex_Mutex) + (LwU32(Index_I)*80) + (LwU32(Index_J)*4) ))
    with Inline_Always,
    Global => null,
     Depends => ( Lw_Sse_Se_Common_Mutex_Mutex_Addr'Result => Index_I,
                  Lw_Sse_Se_Common_Mutex_Mutex_Addr'Result => Index_J);
------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_SSE_SE_COMMON_MUTEX_MUTEX_VALUE_FIELD is new LwU8;
    Lw_Sse_Se_Common_Mutex_Mutex_Value_Init :
        constant LW_SSE_SE_COMMON_MUTEX_MUTEX_VALUE_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_SSE_SE_COMMON_MUTEX_MUTEX_REGISTER is
    record
        F_Value    : LW_SSE_SE_COMMON_MUTEX_MUTEX_VALUE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_SSE_SE_COMMON_MUTEX_MUTEX_REGISTER use
    record
        F_Value at 0 range 0 .. 7;
    end record;

------------------------------------------------------------------------------------------------------
end Dev_Se_Seb_Ada;
