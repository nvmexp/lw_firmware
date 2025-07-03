-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types;          use Lw_Types;
with Types;             use Types;
with Separation_Kernel; use Separation_Kernel;
with Riscv_Core;        use Riscv_Core;
with Riscv_Core.Rv_Gpr;
with Riscv_Core.Rv_Csr;
with Riscv_Core.Csb_Reg;
with Trace_Buffer;     use Trace_Buffer;

package SBI_Switch_To
is

    procedure Switch_To(Param1    : LwU64;
                        Param2    : LwU64;
                        Param3    : LwU64;
                        Param4    : LwU64;
                        Param5    : LwU64;
                        Param6    : LwU64)
    with
        Pre => (Riscv_Core.Ghost_Lwrrent_Stack = Riscv_Core.SK_Stack),
        Global => ( Input  => Trace_Buffer_State,
                    In_Out => (
                        Separation_Kernel_State,
                        Riscv_Core.Ghost_Lwrrent_Stack,
                        Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State,
                        Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State),
                    Output => (
                        Riscv_Core.Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State)),
        No_Return;

end SBI_Switch_To;
