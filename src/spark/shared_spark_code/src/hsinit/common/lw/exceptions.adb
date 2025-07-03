-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Exceptions_Falc_Specific_Tu10x; use Exceptions_Falc_Specific_Tu10x;
with Ada.Unchecked_Colwersion;
with Utility; use Utility;
with Dev_Falc_Spr; use Dev_Falc_Spr;
with Lw_Types.Falcon_Types; use Lw_Types.Falcon_Types;
with Lw_Types.Shift_Right_Op; use Lw_Types.Shift_Right_Op;

package body Exceptions
with SPARK_Mode => On
is

   pragma Assert ( LW_FALC_SEC_SPR_Register'Size = LwU32'Size );
   function From_Raw is new Ada.Unchecked_Colwersion (Source => LwU32,
                                                      Target => LW_FALC_SEC_SPR_Register);

   pragma Assert ( LW_FALC_SEC_SPR_Register'Size = LwU32'Size );
   function To_Raw is new Ada.Unchecked_Colwersion (Source => LW_FALC_SEC_SPR_Register,
                                                    Target => LwU32);

   procedure Exception_Installer_Hang(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is

      pragma Suppress(All_Checks);

      Sec_Spr : LW_FALC_SEC_SPR_Register;

   begin

      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Set_Ibrkpt_Registers(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;


         -- Clear IE0 bit in CSW.
         Falc_Sclrb_C( FALCON_CSW_INTERRUPT0_EN_BIT );

         -- Clear IE1 bit in CSW.
         Falc_Sclrb_C( FALCON_CSW_INTERRUPT1_EN_BIT );

         -- Clear IE2 bit in CSW.
         Falc_Sclrb_C( FALCON_CSW_INTERRUPT2_EN_BIT );

         --  Clear Exception bit in CSW, so that exceptions will jump to EV.
         Falc_Sclrb_C( FALCON_CSW_EXCEPTION_BIT );

         --  Model CSW.E state as a Global Ghost state.
         Ghost_Csw_Spr_Status := CSW_EXCEPTION_BIT_DISABLE;

         Install_Exception_Handler_Hang_To_Ev;

         --Set Disable Exceptions
         -- TODO : Check why we are hetting this issue
         pragma Warnings(Off,"unused assignment",Reason => "Expected as it is RMW");
         Sec_Spr := From_Raw( Read_Sec_Spr_C );
         pragma Warnings(On,"unused assignment",Reason => "Expected as it is RMW");
         Sec_Spr.Disable_Exceptions := Clear;
         Write_Sec_Spr_C(To_Raw(Sec_Spr));

         Ghost_Hs_Init_States_Tracker := EXCEPTION_INSTALLER_HANG;
      end loop Main_Block;

   end Exception_Installer_Hang;

   function Get_Stack_Bottom return LwU32
   is
      Stack_Bottom : LwU32;
   begin
      Stack_Bottom := PUB_STACK_BOTTOM_LIMIT;
      Stack_Bottom := Safe_Shift_Right(Value  => Stack_Bottom,
                                       Amount => 2);
      return Stack_Bottom;
   end Get_Stack_Bottom;

   procedure Exception_Handler_Selwre_Hang
   is
   begin
      Infinite_Loop;
   end Exception_Handler_Selwre_Hang;

end Exceptions;
