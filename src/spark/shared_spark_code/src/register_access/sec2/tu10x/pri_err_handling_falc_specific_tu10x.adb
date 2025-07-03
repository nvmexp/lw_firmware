-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Dev_Sec_Csb_Ada; use Dev_Sec_Csb_Ada;
with Lw_Types; use Lw_Types;
with Ada.Unchecked_Colwersion;
with Utility; use Utility;

package body Pri_Err_Handling_Falc_Specific_Tu10x
with SPARK_Mode => On
is

   function UC_CSBERRSTAT is new Ada.Unchecked_Colwersion(Source => LwU32,
                                                          Target => LW_CSEC_FALCON_CSBERRSTAT_REGISTER);

   procedure Check_Csb_Err_Value(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Val : LwU32;
      CsbErrStatVal : LW_CSEC_FALCON_CSBERRSTAT_REGISTER;
   begin
      Falc_Ldx_I_C( Addr   => Lw_Csec_Falcon_Csberrstat,
                    Bitnum => 0,
                    Val    => Val );

      CsbErrStatVal := UC_CSBERRSTAT(Val);
      if CsbErrStatVal.F_Valid = Lw_Csec_Falcon_Csberrstat_Valid_True then
         -- Set Global variable CSB PRI ERROR STATE to True if we hit CSB PRI Error
         Ghost_Csb_Pri_Error_State := CSB_PRI_ERROR_TRUE;
         Status := FLCN_PRI_ERROR;
      else
         Status := UCODE_ERR_CODE_NOERROR;
      end if;
   end Check_Csb_Err_Value;

end Pri_Err_Handling_Falc_Specific_Tu10x;
