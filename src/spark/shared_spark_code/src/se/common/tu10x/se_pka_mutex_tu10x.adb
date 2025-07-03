-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Falcon_Selwrebusdio; use Falcon_Selwrebusdio;
with Dev_Se_Seb_Ada; use Dev_Se_Seb_Ada;
with Ada.Unchecked_Colwersion;
with Lw_Types; use Lw_Types;
with Se_Pka_Mutex_Falc_Specific_Tu10x; use Se_Pka_Mutex_Falc_Specific_Tu10x;

package body Se_Pka_Mutex_Tu10x
with SPARK_Mode => On
is

   procedure Se_Release_Pka_Mutex( Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
   is
      pragma Suppress(All_Checks);

      Ctrl_Pka_Mutex_Reg : LW_SSE_SE_CTRL_PKA_MUTEX_REGISTER;

      function UC_PKA_MUTEX_TO_LwU32 is new Ada.Unchecked_Colwersion (Source => LW_SSE_SE_CTRL_PKA_MUTEX_REGISTER,
                                                                      Target => LwU32);
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         -- Ghost variables are allowed these values here as an exception since the parent procedure
         -- is called in case of erros as well. May be find a better way.
         Ghost_Csb_Pri_Error_State := CSB_PRI_ERROR_FALSE;

         Ctrl_Pka_Mutex_Reg.F_Se_Lock := Lw_Sse_Se_Ctrl_Pka_Mutex_Se_Lock_Release;
         Ctrl_Pka_Mutex_Reg.F_Rc_Reason := Lw_Sse_Se_Ctrl_Pka_Mutex_Rc_Reason_Scrubbing;
         Selwre_Bus_Write_Register( Lw_Sse_Se_Ctrl_Pka_Mutex, UC_PKA_MUTEX_TO_LwU32( Ctrl_Pka_Mutex_Reg ),
                                    Status );
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- Global ghost variable is set to indicate Mutex Status
         Ghost_Pka_Mutex_Status := MUTEX_RELEASED;
      end loop Main_Block;

   end Se_Release_Pka_Mutex;


   procedure Se_Acquire_Pka_Mutex( Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
   is
   pragma Suppress(All_Checks);

      Pka_Mutex_Reg              : LW_SSE_SE_CTRL_PKA_MUTEX_REGISTER;
      Pka_Mutex_Tmout_Action_Reg : LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION_REGISTER;
      Pka_Mutex_Tmout_Dftval_Reg : LwU32;
      Timestamp_Initial          : LwU32;
      Temp                       : LwU32;
      Check                      : LwU32;
      Timestamp                  : LwU32;

      function UC_SE_MUTEX_TMOUT_ACTION_TO_LwU32 is new Ada.Unchecked_Colwersion
        (Source => LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION_REGISTER,
         Target => LwU32);
      function UC_PKA_MUTEX_FROM_LwU32 is new Ada.Unchecked_Colwersion (Source => LwU32,
                                                                        Target => LW_SSE_SE_CTRL_PKA_MUTEX_REGISTER);
   begin

      Main_Block :
      for Unused_Loop_Var in 1 .. 1 loop

         Selwre_Bus_Read_Register( Lw_Sse_Se_Ctrl_Se_Mutex_Tmout_Dftval, Pka_Mutex_Tmout_Dftval_Reg, Status );
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         if Pka_Mutex_Tmout_Dftval_Reg /= LwU32( Lw_Sse_Se_Ctrl_Se_Mutex_Tmout_Dftval_Tmout_Max ) then
            -- Ghost variable Ghost_Se_Status is updated to SE Bus Error status
            Ghost_Pka_Se_Mutex := PKA_SE_ERROR;
            Status := SEBUS_DEFAULT_TIMEOUT_ILWALID;
            exit Main_Block;
         end if;

         Read_Timestamp(Timestamp_Initial, Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         loop
            pragma Loop_Ilwariant (Status = UCODE_ERR_CODE_NOERROR and then
                                   Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE);

            Selwre_Bus_Read_Register( Lw_Sse_Se_Ctrl_Pka_Mutex, Temp, Status );
            exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

            Pka_Mutex_Reg := UC_PKA_MUTEX_FROM_LwU32( Temp );
            Check := LwU32( Pka_Mutex_Reg.F_Se_Lock );

            Read_Timestamp(Timestamp, Status);
            exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

            -- Is it better to use a if condition instead of this??
            pragma Assume(Timestamp > Timestamp_Initial );
            exit when Check = LW_SSE_SE_CTRL_PKA_MUTEX_RC_SUCCESS or else
              ( ( Timestamp - Timestamp_Initial )
               > SE_PKA_MUTEX_TIMEOUT_VAL_USEC );
         end loop;

         if Check /= LW_SSE_SE_CTRL_PKA_MUTEX_RC_SUCCESS
         then
            -- Ghost variable Ghost_Se_Status is updated to reflect SE Bus Error status
            Ghost_Pka_Se_Mutex := PKA_SE_ERROR;
            Status := SEBUS_ERR_MUTEX_ACQUIRE;
            exit Main_Block;
         end if;

         Pka_Mutex_Tmout_Action_Reg.F_Interrupt := Lw_Sse_Se_Ctrl_Pka_Mutex_Tmout_Action_Interrupt_Enable;
         Pka_Mutex_Tmout_Action_Reg.F_Release_Mutex := Lw_Sse_Se_Ctrl_Pka_Mutex_Tmout_Action_Release_Mutex_Enable;
         Pka_Mutex_Tmout_Action_Reg.F_Reset_Pka := Lw_Sse_Se_Ctrl_Pka_Mutex_Tmout_Action_Reset_Pka_Enable;
         Pka_Mutex_Tmout_Action_Reg.F_Status := Lw_Sse_Se_Ctrl_Pka_Mutex_Tmout_Action_Status_False;

         Selwre_Bus_Write_Register( Lw_Sse_Se_Ctrl_Pka_Mutex_Tmout_Action,
                                    UC_SE_MUTEX_TMOUT_ACTION_TO_LwU32( Pka_Mutex_Tmout_Action_Reg ), Status );
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- Global ghost variable is set to indicate Mutex Status
         Ghost_Pka_Mutex_Status := MUTEX_ACQUIRED;
         pragma Assert (Status = UCODE_ERR_CODE_NOERROR);
      end loop Main_Block;

   end Se_Acquire_Pka_Mutex;

end Se_Pka_Mutex_Tu10x;
