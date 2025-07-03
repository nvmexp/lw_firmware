-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Dev_Riscv_Csr_64;          use Dev_Riscv_Csr_64;
with Lw_Types;                  use Lw_Types;
with Riscv_Core;                use Riscv_Core;
with Riscv_Core.Rv_Gpr;
with Riscv_Core.Rv_Csr;
with Riscv_Core.Csb_Reg;
with Separation_Kernel;
with Trace_Buffer;              use Trace_Buffer;

package SBI
is

    SBI_Version : constant LwU64 := 0;

    procedure Dispatch(arg0        : in out LwU64;
                       arg1        : in out LwU64;
                       arg2        : LwU64;
                       arg3        : LwU64;
                       arg4        : LwU64;
                       arg5        : LwU64;
                       functionId  : LwU64;
                       extensionId : LwU64)
    with
        Annotate => (GNATprove, Might_Not_Return),  -- in case of SHUTDOWN and SWITCH_TO Dispatch won't return, but will for timer SBI_EXTENSION_SET_TIMER
        Global   => (In_Out => (Separation_Kernel.Separation_Kernel_State,
                                Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State,
                                Riscv_Core.Ghost_Lwrrent_Stack,
                                Riscv_Core.Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State,
                                Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State),
                     Input  => Trace_Buffer_State),
        Pre => (Riscv_Core.Ghost_Lwrrent_Stack = Riscv_Core.SK_Stack);

end SBI;
