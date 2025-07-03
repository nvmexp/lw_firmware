-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types;          use Lw_Types;
with Separation_Kernel; use Separation_Kernel;
with Riscv_Core;
with Riscv_Core.Rv_Gpr;
with Riscv_Core.Rv_Csr;
with Riscv_Core.Csb_Reg;
with Trace_Buffer;      use Trace_Buffer;

package Trap
is

    function Get_Address_Of_Trap_Entry return LwU64
    with
        Post => (Get_Address_Of_Trap_Entry'Result mod 4 = 0),
        Inline_Always;

    -- -----------------------------------------------------------------------------
    -- *** SK entry point. Handler for Non-delegated interrupts/exceptions
    -- -----------------------------------------------------------------------------
    procedure Trap_Handler
    with
        Global => ( Output => ( Riscv_Core.Ghost_Lwrrent_Stack,
                                Trace_Buffer_State),
                    In_Out => ( Separation_Kernel_State,
                                Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State,
                                Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State,
                                Riscv_Core.Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State)),
        No_Return,
        Export        => True,
        Convention    => C,
        External_Name => "trap_handler";

end Trap;
