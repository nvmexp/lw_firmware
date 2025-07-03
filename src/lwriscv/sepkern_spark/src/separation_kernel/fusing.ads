-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Error_Handling; use Error_Handling;
with Riscv_Core;

package Fusing is

   -- If we are not in debug_mode then
   -- - Make sure privsec is on and
   -- - RISCV is not in devmode and 
   -- - br_error is disabled
   -- - DCLS is on with action halt
   procedure Check_Fusing(Err_Code : in out Error_Codes) with 
     Pre => Err_Code = OK,
     Global => (Input => Riscv_Core.Bar0_Reg.Bar0_Reg_Hw_Model.Mmio_State),
     Inline_Always;

end Fusing;
