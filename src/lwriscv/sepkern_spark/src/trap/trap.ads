-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Riscv_Core;

package Trap is

   function Get_Address_Of_Trap_Entry return LwU64
     with
       Post => (Get_Address_Of_Trap_Entry'Result mod 4 = 0),
       Inline_Always;

   function Get_Address_Of_Temp_S_Mode_Trap_Handler return LwU64
     with
       Post => (Get_Address_Of_Temp_S_Mode_Trap_Handler'Result mod 4 = 0),
       Inline_Always;

-- -----------------------------------------------------------------------------
-- *** FMC/SK - SK entry point. Handler for Non-delegated interrupts/exceptions
-- -----------------------------------------------------------------------------
   procedure Trap_Handler with
     Global => (In_Out => (Riscv_Core.Ghost_Lwrrent_Stack,
                           Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State,
                           Riscv_Core.Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State)),
     No_Return,
     Export        => True,
     Convention    => C,
     External_Name => "trap_handler";

-- -----------------------------------------------------------------------------
-- *** Temporary Trap Handler for S-Mode - until partition loads its own
-- !!! Address leaked to S-Mode !!!
-- -----------------------------------------------------------------------------
   procedure Temp_S_Mode_Trap_Handler with
     No_Return,
     Export        => True,
     Convention    => C,
     External_Name => "temp_s_mode_trap_handler";

end Trap;
