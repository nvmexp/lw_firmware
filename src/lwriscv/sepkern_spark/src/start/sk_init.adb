-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Error_Handling; use Error_Handling;
with Separation_Kernel;
with Riscv_Core;

procedure SK_Init with
  SPARK_Mode => On,
  Global => (In_out => (Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State,
                        Riscv_Core.Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State,
                        Riscv_Core.Bar0_Reg.Bar0_Reg_Hw_Model.Mmio_State),
             Output => Riscv_Core.Ghost_Lwrrent_Stack),
  No_Return
is
   Err_Code : Error_Codes := OK;
begin
   Riscv_Core.Ghost_Switch_To_SK_Stack;

   Operation_Done:
   for Unused_Loop_Var in 1..1 loop

      -- 1. Verify_HW_State
      Separation_Kernel.Verify_HW_State(Err_Code => Err_Code);
      exit Operation_Done when Err_Code /= OK;

      -- 2. Initialize_SK_State
      Separation_Kernel.Initialize_SK_State(Err_Code => Err_Code);
      exit Operation_Done when Err_Code /= OK;

      -- 3. Verify Fusing
      Separation_Kernel.Verify_Fusing(Err_Code => Err_Code);
      exit Operation_Done when Err_Code /= OK;

      -- 4. Initialize_Partition_CSR
      Separation_Kernel.Initialize_Partition_CSR(Err_Code => Err_Code);
      exit Operation_Done when Err_Code /= OK;

      -- 5. Initialize_Peregrine_State
      Separation_Kernel.Initialize_Peregrine_State(Err_Code => Err_Code);
      exit Operation_Done when Err_Code /= OK;

      -- 6. Load_Initial_Arguments
      Separation_Kernel.Load_Initial_Arguments(Err_Code => Err_Code);
      exit Operation_Done when Err_Code /= OK;

      -- 7. Do Fence IO and Check for Pending Interrupts
      Separation_Kernel.Do_Fence_IO_And_Check_For_Pending_Interrupts(Err_Code => Err_Code);
      exit Operation_Done when Err_Code /= OK;

      -- 8. Clear_GPRs_Phase
      Separation_Kernel.Clear_GPRs(Err_Code => Err_Code);
      exit Operation_Done when Err_Code /= OK;

      -- 9. Transfer to S mode
      Separation_Kernel.Transfer_to_S_Mode;

   end loop Operation_Done;

   Throw_Critical_Error (Pz_Code  => ENTRY_PHASE,
                         Err_Code => CRITICAL_ERROR);

end SK_Init;
