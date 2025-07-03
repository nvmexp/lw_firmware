-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types;          use Lw_Types;
with Error_Handling;    use Error_Handling;
with Riscv_Core;        use Riscv_Core;
with Riscv_Core.Rv_Gpr;
with Riscv_Core.Rv_Csr;
with Riscv_Core.Csb_Reg;
with Separation_Kernel;
with Hw_State_Check;

procedure SK_Init
with
    SPARK_Mode => On,
    Global => (In_out => (Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State,
                          Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State),
               Output => (Separation_Kernel.Separation_Kernel_State,
                          Riscv_Core.Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State,
                          Riscv_Core.Ghost_Lwrrent_Stack)),
    No_Return
is
    Err_Code : Error_Codes := OK;
begin
    Riscv_Core.Ghost_Switch_To_SK_Stack;

    Separation_Kernel.Verify_HW_State(Err_Code => Err_Code);
    if Err_Code /= OK then
        Throw_Critical_Error(Pz_Code  => START_PHASE,
                             Err_Code => Err_Code);
    end if;

    Hw_State_Check.Check_Hw_Engine_State(Err_Code);
    if Err_Code /= OK then
        Throw_Critical_Error(Pz_Code  => START_PHASE,
                             Err_Code => Err_Code);
    end if;

    Separation_Kernel.Initialize_SK_State;

    Separation_Kernel.Initialize_Partition_With_ID(Id => 0);

    Separation_Kernel.Initialize_Peregrine_State;

    Separation_Kernel.Clear_SK_State;

    Separation_Kernel.Check_For_Pending_Interrupts(Err_Code => Err_Code);
    if Err_Code /= OK then
        Throw_Critical_Error(Pz_Code  => START_PHASE,
                             Err_Code => Err_Code);
    end if;

    Separation_Kernel.Load_Initial_Arguments;

    Separation_Kernel.Clear_GPRs;

    Separation_Kernel.Transfer_to_S_Mode;

end SK_Init;
