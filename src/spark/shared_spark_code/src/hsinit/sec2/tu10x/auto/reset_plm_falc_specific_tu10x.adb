-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_


package body Reset_Plm_Falc_Specific_Tu10x
with SPARK_Mode => On
is

   procedure Update_Falcon_Reset_PLM(isRaise : LwBool; Status : out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
   begin
      -- Todo : check ccg/ binary output
      if isRaise = LW_TRUE then
         Status := UCODE_ERR_CODE_NOERROR;
      else
         Status := UCODE_ERR_CODE_NOERROR;
      end if;

      -- Update HS Init state in ghost variable for SPARK Prover
      Ghost_Hs_Init_States_Tracker := RESET_PLM_PROTECTED;

   end Update_Falcon_Reset_PLM;

end Reset_Plm_Falc_Specific_Tu10x;
