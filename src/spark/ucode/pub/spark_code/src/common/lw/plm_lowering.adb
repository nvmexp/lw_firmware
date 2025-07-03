-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Update_Fecs_Plms_Tu10x; use Update_Fecs_Plms_Tu10x;
with Update_Gpcs_Plms_Tu10x; use Update_Gpcs_Plms_Tu10x;
with Update_Pmu_Plms_Tu10x; use Update_Pmu_Plms_Tu10x;

---------------------------------- NOTE START-------------------------------

-- TODO : Sahilc

-- This code has been tested on TU104 silicon via mods test
-- This code has not yet been proved.

-- Thanks !!

---------------------------------- NOTE END-------------------------------



package body Plm_Lowering
with SPARK_Mode => On
is

   procedure Lower_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Lower_Fecs_Plms(Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Lower_Gpcs_Plms(Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Lower_Pmu_Plms(Status);

      end loop Main_Block;

   end Lower_Plms;


end Plm_Lowering;
