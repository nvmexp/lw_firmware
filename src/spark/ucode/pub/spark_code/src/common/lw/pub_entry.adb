-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_
with Plm_Lowering; use Plm_Lowering;
with Pmu_Reset; use Pmu_Reset;
with Dev_Fbpa_War; use Dev_Fbpa_War;

package body Pub_Entry
with SPARK_Mode => On
is

   procedure Pub_Core_Entry(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
   begin
      Main_Block :
      for Unused_Loop_Var in 1 .. 1 loop

         -- Reset PMU before setting PLMs for pmu
         Reset_Pmu(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;
         Lower_Plms(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Pub_Provide_Access_Fbpa(Status => Status);
      end loop Main_Block;

   end Pub_Core_Entry;


end Pub_Entry;
