-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;

--******************************  DEV_FALCON_V4  ******************************--

package Dev_Falcon_V4
with SPARK_MODE => On
is

   LW_FALCON_LWDEC_BASE    : constant := 16#0084_8000#;
   LW_FALCON_LWDEC0_BASE   : constant := 16#0084_8000#;
   LW_FALCON_LWDEC1_BASE   : constant := 16#0084_c000#;
   LW_FALCON_MSVLD_BASE    : constant := 16#0008_4000#;
   LW_FALCON_MSPDEC_BASE   : constant := 16#0008_5000#;
   LW_FALCON_MSPPP_BASE    : constant := 16#0008_6000#;
   LW_FALCON_PWR_BASE      : constant := 16#0010_a000#;
   LW_FALCON_CE0_BASE      : constant := 16#0010_4000#;
   LW_FALCON_CE1_BASE      : constant := 16#0010_5000#;
   LW_FALCON_LWENC_BASE    : constant := 16#001c_8000#;
   LW_FALCON_LWENC0_BASE   : constant := 16#001c_8000#;
   LW_FALCON_HDA_BASE      : constant := 16#001c_3000#;
   LW_FALCON_GSP_BASE      : constant := 16#0011_0000#;
   LW_FALCON_MINION_BASE   : constant := 16#00A0_6000#;
   LW_FALCON_FBFALCON_BASE : constant := 16#009A_4000#;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pfalcon_Falcon_Mailbox0    : constant Bar0_Addr := 16#0000_0040#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PFALCON_FALCON_MAILBOX0_DATA_FIELD is new LwU32;
    Lw_Pfalcon_Falcon_Mailbox0_Data_Init :
        constant LW_PFALCON_FALCON_MAILBOX0_DATA_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PFALCON_FALCON_MAILBOX0_REGISTER is
    record
        F_Data    : LW_PFALCON_FALCON_MAILBOX0_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FALCON_MAILBOX0_REGISTER use
    record
        F_Data at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pfalcon_Falcon_Mailbox1    : constant Bar0_Addr := 16#0000_0044#;

------------------------------------------------------------------------------------------------------
---------- Record Field Type Declaration ----------

    type LW_PFALCON_FALCON_MAILBOX1_DATA_FIELD is new LwU32;
    Lw_Pfalcon_Falcon_Mailbox1_Data_Init :
        constant LW_PFALCON_FALCON_MAILBOX1_DATA_FIELD
        := 16#0000_0000#;

---------- Record Declaration ----------

    type LW_PFALCON_FALCON_MAILBOX1_REGISTER is
    record
        F_Data    : LW_PFALCON_FALCON_MAILBOX1_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FALCON_MAILBOX1_REGISTER use
    record
        F_Data at 0 range 0 .. 31;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pfalcon_Falcon_Dmatrfcmd    : constant Bar0_Addr := 16#0000_0118#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_DMATRFCMD_FULL_FIELD is
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Full_False,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Full_True
    ) with size => 1;

    for LW_PFALCON_FALCON_DMATRFCMD_FULL_FIELD use
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Full_False => 0,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Full_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_DMATRFCMD_IDLE_FIELD is
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Idle_False,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Idle_True
    ) with size => 1;

    for LW_PFALCON_FALCON_DMATRFCMD_IDLE_FIELD use
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Idle_False => 0,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Idle_True => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FALCON_DMATRFCMD_SEC_FIELD is new LwU2;
---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_DMATRFCMD_IMEM_FIELD is
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Imem_False,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Imem_True
    ) with size => 1;

    for LW_PFALCON_FALCON_DMATRFCMD_IMEM_FIELD use
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Imem_False => 0,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Imem_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_DMATRFCMD_WRITE_FIELD is
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Write_False,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Write_True
    ) with size => 1;

    for LW_PFALCON_FALCON_DMATRFCMD_WRITE_FIELD use
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Write_False => 0,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Write_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_DMATRFCMD_NOTIFY_FIELD is
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Notify_False,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Notify_True
    ) with size => 1;

    for LW_PFALCON_FALCON_DMATRFCMD_NOTIFY_FIELD use
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Notify_False => 0,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Notify_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_DMATRFCMD_SIZE_FIELD is
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Size_4B,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Size_8B,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Size_16B,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Size_32B,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Size_64B,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Size_128B,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Size_256B
    ) with size => 3;

    for LW_PFALCON_FALCON_DMATRFCMD_SIZE_FIELD use
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Size_4B => 0,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Size_8B => 1,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Size_16B => 2,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Size_32B => 3,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Size_64B => 4,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Size_128B => 5,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Size_256B => 6
    );

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FALCON_DMATRFCMD_CTXDMA_FIELD is new LwU3;
---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_DMATRFCMD_SET_DMTAG_FIELD is
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Set_Dmtag_Init,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Set_Dmtag_True
    ) with size => 1;

    for LW_PFALCON_FALCON_DMATRFCMD_SET_DMTAG_FIELD use
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Set_Dmtag_Init => 0,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Set_Dmtag_True => 1
    );

    Lw_Pfalcon_Falcon_Dmatrfcmd_Set_Dmtag_False :
        constant LW_PFALCON_FALCON_DMATRFCMD_SET_DMTAG_FIELD
        := Lw_Pfalcon_Falcon_Dmatrfcmd_Set_Dmtag_Init;

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_DMATRFCMD_ERROR_FIELD is
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Error_False,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Error_True
    ) with size => 1;

    for LW_PFALCON_FALCON_DMATRFCMD_ERROR_FIELD use
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Error_False => 0,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Error_True => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FALCON_DMATRFCMD_LVL_FIELD is new LwU3;
---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_DMATRFCMD_SET_DMLVL_FIELD is
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Set_Dmlvl_Init,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Set_Dmlvl_True
    ) with size => 1;

    for LW_PFALCON_FALCON_DMATRFCMD_SET_DMLVL_FIELD use
    (
        Lw_Pfalcon_Falcon_Dmatrfcmd_Set_Dmlvl_Init => 0,
        Lw_Pfalcon_Falcon_Dmatrfcmd_Set_Dmlvl_True => 1
    );

    Lw_Pfalcon_Falcon_Dmatrfcmd_Set_Dmlvl_False :
        constant LW_PFALCON_FALCON_DMATRFCMD_SET_DMLVL_FIELD
        := Lw_Pfalcon_Falcon_Dmatrfcmd_Set_Dmlvl_Init;

---------- Record Declaration ----------

    type LW_PFALCON_FALCON_DMATRFCMD_REGISTER is
    record
        F_Full    : LW_PFALCON_FALCON_DMATRFCMD_FULL_FIELD;
        F_Idle    : LW_PFALCON_FALCON_DMATRFCMD_IDLE_FIELD;
        F_Sec    : LW_PFALCON_FALCON_DMATRFCMD_SEC_FIELD;
        F_Imem    : LW_PFALCON_FALCON_DMATRFCMD_IMEM_FIELD;
        F_Write    : LW_PFALCON_FALCON_DMATRFCMD_WRITE_FIELD;
        F_Notify    : LW_PFALCON_FALCON_DMATRFCMD_NOTIFY_FIELD;
        F_Size    : LW_PFALCON_FALCON_DMATRFCMD_SIZE_FIELD;
        F_Ctxdma    : LW_PFALCON_FALCON_DMATRFCMD_CTXDMA_FIELD;
        F_Set_Dmtag    : LW_PFALCON_FALCON_DMATRFCMD_SET_DMTAG_FIELD;
        F_Error    : LW_PFALCON_FALCON_DMATRFCMD_ERROR_FIELD;
        F_Lvl    : LW_PFALCON_FALCON_DMATRFCMD_LVL_FIELD;
        F_Set_Dmlvl    : LW_PFALCON_FALCON_DMATRFCMD_SET_DMLVL_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FALCON_DMATRFCMD_REGISTER use
    record
        F_Full at 0 range 0 .. 0;
        F_Idle at 0 range 1 .. 1;
        F_Sec at 0 range 2 .. 3;
        F_Imem at 0 range 4 .. 4;
        F_Write at 0 range 5 .. 5;
        F_Notify at 0 range 6 .. 6;
        F_Size at 0 range 8 .. 10;
        F_Ctxdma at 0 range 12 .. 14;
        F_Set_Dmtag at 0 range 16 .. 16;
        F_Error at 0 range 20 .. 20;
        F_Lvl at 0 range 24 .. 26;
        F_Set_Dmlvl at 0 range 27 .. 27;
    end record;

------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pfalcon_Falcon_Engctl    : constant Bar0_Addr := 16#0000_00A4#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_ENGCTL_ILW_CONTEXT_FIELD is
    (
        Lw_Pfalcon_Falcon_Engctl_Ilw_Context_False,
        Lw_Pfalcon_Falcon_Engctl_Ilw_Context_True
    ) with size => 1;

    for LW_PFALCON_FALCON_ENGCTL_ILW_CONTEXT_FIELD use
    (
        Lw_Pfalcon_Falcon_Engctl_Ilw_Context_False => 0,
        Lw_Pfalcon_Falcon_Engctl_Ilw_Context_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_ENGCTL_SET_STALLREQ_FIELD is
    (
        Lw_Pfalcon_Falcon_Engctl_Set_Stallreq_False,
        Lw_Pfalcon_Falcon_Engctl_Set_Stallreq_True
    ) with size => 1;

    for LW_PFALCON_FALCON_ENGCTL_SET_STALLREQ_FIELD use
    (
        Lw_Pfalcon_Falcon_Engctl_Set_Stallreq_False => 0,
        Lw_Pfalcon_Falcon_Engctl_Set_Stallreq_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_ENGCTL_CLR_STALLREQ_FIELD is
    (
        Lw_Pfalcon_Falcon_Engctl_Clr_Stallreq_False,
        Lw_Pfalcon_Falcon_Engctl_Clr_Stallreq_True
    ) with size => 1;

    for LW_PFALCON_FALCON_ENGCTL_CLR_STALLREQ_FIELD use
    (
        Lw_Pfalcon_Falcon_Engctl_Clr_Stallreq_False => 0,
        Lw_Pfalcon_Falcon_Engctl_Clr_Stallreq_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_ENGCTL_SWITCH_CONTEXT_FIELD is
    (
        Lw_Pfalcon_Falcon_Engctl_Switch_Context_False,
        Lw_Pfalcon_Falcon_Engctl_Switch_Context_True
    ) with size => 1;

    for LW_PFALCON_FALCON_ENGCTL_SWITCH_CONTEXT_FIELD use
    (
        Lw_Pfalcon_Falcon_Engctl_Switch_Context_False => 0,
        Lw_Pfalcon_Falcon_Engctl_Switch_Context_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_ENGCTL_STALLREQ_FIELD is
    (
        Lw_Pfalcon_Falcon_Engctl_Stallreq_False,
        Lw_Pfalcon_Falcon_Engctl_Stallreq_True
    ) with size => 1;

    for LW_PFALCON_FALCON_ENGCTL_STALLREQ_FIELD use
    (
        Lw_Pfalcon_Falcon_Engctl_Stallreq_False => 0,
        Lw_Pfalcon_Falcon_Engctl_Stallreq_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_ENGCTL_STALLACK_FIELD is
    (
        Lw_Pfalcon_Falcon_Engctl_Stallack_False,
        Lw_Pfalcon_Falcon_Engctl_Stallack_True
    ) with size => 1;

    for LW_PFALCON_FALCON_ENGCTL_STALLACK_FIELD use
    (
        Lw_Pfalcon_Falcon_Engctl_Stallack_False => 0,
        Lw_Pfalcon_Falcon_Engctl_Stallack_True => 1
    );

---------- Record Declaration ----------

    type LW_PFALCON_FALCON_ENGCTL_REGISTER is
    record
        F_Ilw_Context    : LW_PFALCON_FALCON_ENGCTL_ILW_CONTEXT_FIELD;
        F_Set_Stallreq    : LW_PFALCON_FALCON_ENGCTL_SET_STALLREQ_FIELD;
        F_Clr_Stallreq    : LW_PFALCON_FALCON_ENGCTL_CLR_STALLREQ_FIELD;
        F_Switch_Context    : LW_PFALCON_FALCON_ENGCTL_SWITCH_CONTEXT_FIELD;
        F_Stallreq    : LW_PFALCON_FALCON_ENGCTL_STALLREQ_FIELD;
        F_Stallack    : LW_PFALCON_FALCON_ENGCTL_STALLACK_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FALCON_ENGCTL_REGISTER use
    record
        F_Ilw_Context at 0 range 0 .. 0;
        F_Set_Stallreq at 0 range 1 .. 1;
        F_Clr_Stallreq at 0 range 2 .. 2;
        F_Switch_Context at 0 range 3 .. 3;
        F_Stallreq at 0 range 8 .. 8;
        F_Stallack at 0 range 9 .. 9;
    end record;
------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pfalcon_Falcon_Fhstate    : constant Bar0_Addr := 16#0000_005C#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_FHSTATE_FALCON_HALTED_FIELD is
    (
        Lw_Pfalcon_Falcon_Fhstate_Falcon_Halted_False,
        Lw_Pfalcon_Falcon_Fhstate_Falcon_Halted_True
    ) with size => 1;

    for LW_PFALCON_FALCON_FHSTATE_FALCON_HALTED_FIELD use
    (
        Lw_Pfalcon_Falcon_Fhstate_Falcon_Halted_False => 0,
        Lw_Pfalcon_Falcon_Fhstate_Falcon_Halted_True => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FALCON_FHSTATE_EXT_HALTED_FIELD is new LwU15;
---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_FHSTATE_ENGINE_FAULTED_FIELD is
    (
        Lw_Pfalcon_Falcon_Fhstate_Engine_Faulted_False,
        Lw_Pfalcon_Falcon_Fhstate_Engine_Faulted_True
    ) with size => 1;

    for LW_PFALCON_FALCON_FHSTATE_ENGINE_FAULTED_FIELD use
    (
        Lw_Pfalcon_Falcon_Fhstate_Engine_Faulted_False => 0,
        Lw_Pfalcon_Falcon_Fhstate_Engine_Faulted_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_FHSTATE_STALL_REQ_FIELD is
    (
        Lw_Pfalcon_Falcon_Fhstate_Stall_Req_False,
        Lw_Pfalcon_Falcon_Fhstate_Stall_Req_True
    ) with size => 1;

    for LW_PFALCON_FALCON_FHSTATE_STALL_REQ_FIELD use
    (
        Lw_Pfalcon_Falcon_Fhstate_Stall_Req_False => 0,
        Lw_Pfalcon_Falcon_Fhstate_Stall_Req_True => 1
    );

---------- Record Declaration ----------

    type LW_PFALCON_FALCON_FHSTATE_REGISTER is
    record
        F_Falcon_Halted    : LW_PFALCON_FALCON_FHSTATE_FALCON_HALTED_FIELD;
        F_Ext_Halted    : LW_PFALCON_FALCON_FHSTATE_EXT_HALTED_FIELD;
        F_Engine_Faulted    : LW_PFALCON_FALCON_FHSTATE_ENGINE_FAULTED_FIELD;
        F_Stall_Req    : LW_PFALCON_FALCON_FHSTATE_STALL_REQ_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FALCON_FHSTATE_REGISTER use
    record
        F_Falcon_Halted at 0 range 0 .. 0;
        F_Ext_Halted at 0 range 1 .. 15;
        F_Engine_Faulted at 0 range 16 .. 16;
        F_Stall_Req at 0 range 17 .. 17;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Pfalcon_Falcon_Sctl    : constant Bar0_Addr := 16#0000_0240#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_SCTL_LSMODE_FIELD is
    (
        Lw_Pfalcon_Falcon_Sctl_Lsmode_False,
        Lw_Pfalcon_Falcon_Sctl_Lsmode_True
    ) with size => 1;

    for LW_PFALCON_FALCON_SCTL_LSMODE_FIELD use
    (
        Lw_Pfalcon_Falcon_Sctl_Lsmode_False => 0,
        Lw_Pfalcon_Falcon_Sctl_Lsmode_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_SCTL_HSMODE_FIELD is
    (
        Lw_Pfalcon_Falcon_Sctl_Hsmode_False,
        Lw_Pfalcon_Falcon_Sctl_Hsmode_True
    ) with size => 1;

    for LW_PFALCON_FALCON_SCTL_HSMODE_FIELD use
    (
        Lw_Pfalcon_Falcon_Sctl_Hsmode_False => 0,
        Lw_Pfalcon_Falcon_Sctl_Hsmode_True => 1
    );

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FALCON_SCTL_LSMODE_LEVEL_FIELD is new LwU2;
    Lw_Pfalcon_Falcon_Sctl_Lsmode_Level_Init :
        constant LW_PFALCON_FALCON_SCTL_LSMODE_LEVEL_FIELD
        := 16#0000_0000#;

---------- Record Field Type Declaration ----------

    type LW_PFALCON_FALCON_SCTL_DEBUG_PRIV_LEVEL_FIELD is new LwU2;
    Lw_Pfalcon_Falcon_Sctl_Debug_Priv_Level_Init :
        constant LW_PFALCON_FALCON_SCTL_DEBUG_PRIV_LEVEL_FIELD
        := 16#0000_0000#;

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_SCTL_RESET_LVLM_EN_FIELD is
    (
        Lw_Pfalcon_Falcon_Sctl_Reset_Lvlm_En_False,
        Lw_Pfalcon_Falcon_Sctl_Reset_Lvlm_En_True
    ) with size => 1;

    for LW_PFALCON_FALCON_SCTL_RESET_LVLM_EN_FIELD use
    (
        Lw_Pfalcon_Falcon_Sctl_Reset_Lvlm_En_False => 0,
        Lw_Pfalcon_Falcon_Sctl_Reset_Lvlm_En_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_SCTL_STALLREQ_CLR_EN_FIELD is
    (
        Lw_Pfalcon_Falcon_Sctl_Stallreq_Clr_En_False,
        Lw_Pfalcon_Falcon_Sctl_Stallreq_Clr_En_True
    ) with size => 1;

    for LW_PFALCON_FALCON_SCTL_STALLREQ_CLR_EN_FIELD use
    (
        Lw_Pfalcon_Falcon_Sctl_Stallreq_Clr_En_False => 0,
        Lw_Pfalcon_Falcon_Sctl_Stallreq_Clr_En_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_SCTL_AUTH_EN_FIELD is
    (
        Lw_Pfalcon_Falcon_Sctl_Auth_En_False,
        Lw_Pfalcon_Falcon_Sctl_Auth_En_True
    ) with size => 1;

    for LW_PFALCON_FALCON_SCTL_AUTH_EN_FIELD use
    (
        Lw_Pfalcon_Falcon_Sctl_Auth_En_False => 0,
        Lw_Pfalcon_Falcon_Sctl_Auth_En_True => 1
    );

---------- Record Declaration ----------

    type LW_PFALCON_FALCON_SCTL_REGISTER is
    record
        F_Lsmode    : LW_PFALCON_FALCON_SCTL_LSMODE_FIELD;
        F_Hsmode    : LW_PFALCON_FALCON_SCTL_HSMODE_FIELD;
        F_Lsmode_Level    : LW_PFALCON_FALCON_SCTL_LSMODE_LEVEL_FIELD;
        F_Debug_Priv_Level    : LW_PFALCON_FALCON_SCTL_DEBUG_PRIV_LEVEL_FIELD;
        F_Reset_Lvlm_En    : LW_PFALCON_FALCON_SCTL_RESET_LVLM_EN_FIELD;
        F_Stallreq_Clr_En    : LW_PFALCON_FALCON_SCTL_STALLREQ_CLR_EN_FIELD;
        F_Auth_En    : LW_PFALCON_FALCON_SCTL_AUTH_EN_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FALCON_SCTL_REGISTER use
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

    Lw_Pfalcon_Falcon_Itfen    : constant Bar0_Addr := 16#0000_0048#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_ITFEN_CTXEN_FIELD is
    (
        Lw_Pfalcon_Falcon_Itfen_Ctxen_Init,
        Lw_Pfalcon_Falcon_Itfen_Ctxen_Enable
    ) with size => 1;

    for LW_PFALCON_FALCON_ITFEN_CTXEN_FIELD use
    (
        Lw_Pfalcon_Falcon_Itfen_Ctxen_Init => 0,
        Lw_Pfalcon_Falcon_Itfen_Ctxen_Enable => 1
    );

    Lw_Pfalcon_Falcon_Itfen_Ctxen_Disable :
        constant LW_PFALCON_FALCON_ITFEN_CTXEN_FIELD
        := Lw_Pfalcon_Falcon_Itfen_Ctxen_Init;

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_ITFEN_MTHDEN_FIELD is
    (
        Lw_Pfalcon_Falcon_Itfen_Mthden_Init,
        Lw_Pfalcon_Falcon_Itfen_Mthden_Enable
    ) with size => 1;

    for LW_PFALCON_FALCON_ITFEN_MTHDEN_FIELD use
    (
        Lw_Pfalcon_Falcon_Itfen_Mthden_Init => 0,
        Lw_Pfalcon_Falcon_Itfen_Mthden_Enable => 1
    );

    Lw_Pfalcon_Falcon_Itfen_Mthden_Disable :
        constant LW_PFALCON_FALCON_ITFEN_MTHDEN_FIELD
        := Lw_Pfalcon_Falcon_Itfen_Mthden_Init;

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_ITFEN_PRIV_POSTWR_FIELD is
    (
        Lw_Pfalcon_Falcon_Itfen_Priv_Postwr_Init,
        Lw_Pfalcon_Falcon_Itfen_Priv_Postwr_True
    ) with size => 1;

    for LW_PFALCON_FALCON_ITFEN_PRIV_POSTWR_FIELD use
    (
        Lw_Pfalcon_Falcon_Itfen_Priv_Postwr_Init => 0,
        Lw_Pfalcon_Falcon_Itfen_Priv_Postwr_True => 1
    );

    Lw_Pfalcon_Falcon_Itfen_Priv_Postwr_False :
        constant LW_PFALCON_FALCON_ITFEN_PRIV_POSTWR_FIELD
        := Lw_Pfalcon_Falcon_Itfen_Priv_Postwr_Init;

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_ITFEN_PRIV_SECWL_CPUCTL_ALIAS_FIELD is
    (
        Lw_Pfalcon_Falcon_Itfen_Priv_Secwl_Cpuctl_Alias_Init,
        Lw_Pfalcon_Falcon_Itfen_Priv_Secwl_Cpuctl_Alias_True
    ) with size => 1;

    for LW_PFALCON_FALCON_ITFEN_PRIV_SECWL_CPUCTL_ALIAS_FIELD use
    (
        Lw_Pfalcon_Falcon_Itfen_Priv_Secwl_Cpuctl_Alias_Init => 0,
        Lw_Pfalcon_Falcon_Itfen_Priv_Secwl_Cpuctl_Alias_True => 1
    );

    Lw_Pfalcon_Falcon_Itfen_Priv_Secwl_Cpuctl_Alias_False :
        constant LW_PFALCON_FALCON_ITFEN_PRIV_SECWL_CPUCTL_ALIAS_FIELD
        := Lw_Pfalcon_Falcon_Itfen_Priv_Secwl_Cpuctl_Alias_Init;

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_ITFEN_CTXSW_NACK_FIELD is
    (
        Lw_Pfalcon_Falcon_Itfen_Ctxsw_Nack_False,
        Lw_Pfalcon_Falcon_Itfen_Ctxsw_Nack_True
    ) with size => 1;

    for LW_PFALCON_FALCON_ITFEN_CTXSW_NACK_FIELD use
    (
        Lw_Pfalcon_Falcon_Itfen_Ctxsw_Nack_False => 0,
        Lw_Pfalcon_Falcon_Itfen_Ctxsw_Nack_True => 1
    );

---------- Enum Declaration ----------

    type LW_PFALCON_FALCON_ITFEN_HIRQ_NONSTALL_EDGE_FIELD is
    (
        Lw_Pfalcon_Falcon_Itfen_Hirq_Nonstall_Edge_False,
        Lw_Pfalcon_Falcon_Itfen_Hirq_Nonstall_Edge_True
    ) with size => 1;

    for LW_PFALCON_FALCON_ITFEN_HIRQ_NONSTALL_EDGE_FIELD use
    (
        Lw_Pfalcon_Falcon_Itfen_Hirq_Nonstall_Edge_False => 0,
        Lw_Pfalcon_Falcon_Itfen_Hirq_Nonstall_Edge_True => 1
    );

---------- Record Declaration ----------

    type LW_PFALCON_FALCON_ITFEN_REGISTER is
    record
        F_Ctxen    : LW_PFALCON_FALCON_ITFEN_CTXEN_FIELD;
        F_Mthden    : LW_PFALCON_FALCON_ITFEN_MTHDEN_FIELD;
        F_Priv_Postwr    : LW_PFALCON_FALCON_ITFEN_PRIV_POSTWR_FIELD;
        F_Priv_Secwl_Cpuctl_Alias    : LW_PFALCON_FALCON_ITFEN_PRIV_SECWL_CPUCTL_ALIAS_FIELD;
        F_Ctxsw_Nack    : LW_PFALCON_FALCON_ITFEN_CTXSW_NACK_FIELD;
        F_Hirq_Nonstall_Edge    : LW_PFALCON_FALCON_ITFEN_HIRQ_NONSTALL_EDGE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FALCON_ITFEN_REGISTER use
    record
        F_Ctxen at 0 range 0 .. 0;
        F_Mthden at 0 range 1 .. 1;
        F_Priv_Postwr at 0 range 2 .. 2;
        F_Priv_Secwl_Cpuctl_Alias at 0 range 4 .. 4;
        F_Ctxsw_Nack at 0 range 8 .. 8;
        F_Hirq_Nonstall_Edge at 0 range 12 .. 12;
    end record;

------------------------------------------------------------------------------------------------------
end Dev_Falcon_V4;
