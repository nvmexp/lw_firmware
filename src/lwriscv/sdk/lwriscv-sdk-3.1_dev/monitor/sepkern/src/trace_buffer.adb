--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Types;                    use Types;
with Dev_Prgnlcl;              use Dev_Prgnlcl;
with Riscv_Core.Csb_Reg;       use Riscv_Core.Csb_Reg;
with PLM_Types;                use PLM_Types;
with Separation_Kernel.Policies; use Separation_Kernel.Policies;

package body Trace_Buffer
with
    Refined_State => (Trace_Buffer_State => (TracePrivMask))
is
    TracePrivMask : LW_PRGNLCL_PRIV_LEVEL_MASK_Register;

    procedure Disable_Trace_Buffer
    is
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_PRIV_LEVEL_MASK_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_PRIV_LEVEL_MASK_Register);

        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_TRACECTL_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_TRACECTL_Register);

        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_TRACE_WTIDX_Register);
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_TRACEPC_LO_Register);
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_TRACEPC_HI_Register);

        TraceCtrl : LW_PRGNLCL_RISCV_TRACECTL_Register;
    begin
        -- Disable mmode trace buffer
        Csb_Reg_Rd32(Addr => LW_PRGNLCL_RISCV_TRACECTL_Address,
                     Data => TraceCtrl);
        TraceCtrl.Mmode_Enable := ENABLE_FALSE;
        Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_TRACECTL_Address,
                     Data => TraceCtrl);

        -- Read and save current trace buffer priv level mask setting
        Csb_Reg_Rd32(Addr => LW_PRGNLCL_RISCV_TRACEBUF_PRIV_LEVEL_MASK_Address,
                     Data => TracePrivMask);

        -- Disable trace buffer priv access
        Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_TRACEBUF_PRIV_LEVEL_MASK_Address,
                     Data => PRIV_LEVEL_MASK_ALL_DISABLE);

    end Disable_Trace_Buffer;

    procedure Restore_Trace_Buffer
    is
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_PRIV_LEVEL_MASK_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_PRIV_LEVEL_MASK_Register);

        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_TRACECTL_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_TRACECTL_Register);

    begin
        -- Restore trace buffer priv access
        Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_TRACEBUF_PRIV_LEVEL_MASK_Address,
                     Data => TracePrivMask);

    end Restore_Trace_Buffer;
end Trace_Buffer;
