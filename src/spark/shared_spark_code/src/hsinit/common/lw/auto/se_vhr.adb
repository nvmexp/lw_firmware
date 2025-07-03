-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Se_Pka_Mutex_Tu10x; use Se_Pka_Mutex_Tu10x;
with Falcon_Selwrebusdio; use Falcon_Selwrebusdio;
with Se_Vhr_Helper_Falc_Specific_Tu10x; use Se_Vhr_Helper_Falc_Specific_Tu10x;
with Lw_Types; use Lw_Types;
with Dev_Se_Seb_Ada; use Dev_Se_Seb_Ada;

package body Se_Vhr
with SPARK_Mode => On
is

   procedure Se_Vhr_Empty_Check( Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
   is

      pragma Suppress(All_Checks);

      Is_Mutex_Acquired    : Boolean := False;
      Se_Val               : LwU32;
      Sse_Addr             : LW_SSE_BAR0_ADDR;
      Status_Exit          : LW_UCODE_UNIFIED_ERROR_TYPE := UCODE_ERR_CODE_NOERROR;
      Ent_Num_Mask         : LwU32;

   begin
      Main_Block :
      for Unused_Loop_Var in 1 .. 1 loop

         Callwlate_Sse_Addr(Sse_Addr => Sse_Addr,
                            Status   => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;


         Callwlate_Ent_Num_Mask(Ent_Num_Mask, Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- TODO : Add the initialize part for SE
         Se_Acquire_Pka_Mutex( Status );
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         pragma Assert (Ghost_Pka_Mutex_Status = MUTEX_ACQUIRED);
         Is_Mutex_Acquired := True;

         Selwre_Bus_Read_Register( Sse_Addr, Se_Val, Status );
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Se_Val := "and"( Se_Val, Ent_Num_Mask );
         if Se_Val /= 0 then
            Ghost_Vhr_Se_Status := VHR_SE_ERROR;
            Status := SEBUS_VHR_CHECK_FAILED;
            exit Main_Block;
         end if;

         -- Update HS Init state in ghost variable for SPARK Prover
         Ghost_Vhr_Status := VHR_CLEARED;
         Ghost_Hs_Init_States_Tracker := VHR_EMPTY_CHECKED;
         Ghost_Halt_Action_State := HALT_ACTION_ALLOWED;

      end loop Main_Block;

      if Is_Mutex_Acquired then
         Se_Release_Pka_Mutex( Status_Exit );
      end if;

      if Status = UCODE_ERR_CODE_NOERROR then
         Status := Status_Exit;
      elsif Status = FLCN_PRI_ERROR then
         Ghost_Csb_Pri_Error_State := CSB_PRI_ERROR_TRUE;
      end if;

   end Se_Vhr_Empty_Check;

end Se_Vhr;
