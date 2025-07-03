-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2019 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_
with Fbpa_War_tu10x; use Fbpa_War_tu10x;
package body Dev_Fbpa_War
is
   procedure Pub_Provide_Access_Fbpa(Status: in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop
         Provide_Access_Fbpa_To_Cpu_Bug_2369597(Status => Status);
      end loop Main_Block;
   end Pub_Provide_Access_Fbpa;
end Dev_Fbpa_War;
