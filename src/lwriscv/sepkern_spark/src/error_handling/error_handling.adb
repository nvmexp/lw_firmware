-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core; 

package body Error_Handling is

   procedure Throw_Critical_Error (Pz_Code : Phase_Codes; Err_Code : Error_Codes) is
      -- Leaving as is until decided what/if anything to do here
      pragma Unreferenced(Pz_Code, Err_Code); 
   begin
      Riscv_Core.Halt;
   end Throw_Critical_Error;

   procedure Last_Chance_Handler (Source_Location : System.Address; Line : Integer) is
      pragma Unreferenced (Line, Source_Location);
   begin
      Riscv_Core.Halt;
   end Last_Chance_Handler;

end Error_Handling;
