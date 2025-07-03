-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with System;
with Riscv_Core;

package Error_Handling is

   -- TODO: Rename this 
   type Phase_Codes is (
                        ENTRY_PHASE,
                        TRAP_HANDLER_PHASE -- Not really a phase
                        );

   type Error_Codes is (OK, CRITICAL_ERROR);

   procedure Throw_Critical_Error (Pz_Code : Phase_Codes; Err_Code : Error_Codes) with
     Pre => Err_Code /= OK,
     Inline_Always,
     No_Return;

   -- Overriding Last Chance Handler (LCH) as recommended
   procedure Last_Chance_Handler (Source_Location : System.Address; Line : Integer) with 
     No_Return,
     Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);
   pragma Export (C, Last_Chance_Handler, "__gnat_last_chance_handler");

end Error_Handling;
