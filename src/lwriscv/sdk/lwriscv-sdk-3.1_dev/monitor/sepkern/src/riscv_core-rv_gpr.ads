-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

package Riscv_Core.Rv_Gpr
is

    -- Gpr_Reg_Hw_Model package
    package Gpr_Reg_Hw_Model
    with
        Abstract_State =>
            (Gpr_State with External => (Async_Readers, Effective_Writes))
    is
    end Gpr_Reg_Hw_Model;
    -----------------------------------------------------------------------

    -- For ra, gp, tp, s0/fp registers. Excluding sp, but including ra
    type Pointer_Registers_Array is array (LwU2 range 0 .. 3) of LwU64;

    -- For t0-t7 registers
    type Temp_Registers_Array is array (LwU3 range 0 .. 6) of LwU64;

    -- For a0-a7 registers
    type Argument_Registers_Array is array (LwU3 range 0 .. 7) of LwU64;

    -- For s1-s11 registers
    type Saved_Registers_Array is array (LwU4 range 0 .. 10) of LwU64;

    -- Set all GPRs to 0.
    procedure Clear_Gpr
    with
        Post => (Riscv_Core.Ghost_Lwrrent_Stack = Zero_SP),
        Global => (Output => (Gpr_Reg_Hw_Model.Gpr_State, Riscv_Core.Ghost_Lwrrent_Stack)),
        Inline_Always;

    procedure Switch_To_Old_Stack (Load_Old_SP_Value_From : LwU32)
    with
        Post => Riscv_Core.Ghost_Lwrrent_Stack = S_Stack,
        Inline_Always;

    procedure Save_Pointer_Registers_To(p : out Pointer_Registers_Array)
    with
        Global => (Input => Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State),
        Inline_Always;

    procedure Restore_Pointer_Registers_From(p : Pointer_Registers_Array)
    with
        Global => (Output => Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State),
        Inline_Always;

    procedure Save_Temp_Registers_To(t : out Temp_Registers_Array)
    with
        Global => (Input => Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State),
        Inline_Always;

    procedure Restore_Temp_Registers_From(t : Temp_Registers_Array)
    with
        Global => (Output => Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State),
        Inline_Always;

    procedure Save_Argument_Registers_To(a : out Argument_Registers_Array)
    with
        Global => (Input => Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State),
        Inline_Always;

    procedure Restore_Argument_Registers_From(a : Argument_Registers_Array)
    with
        Global => (Output => Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State),
        Inline_Always;

    procedure Save_Saved_Registers_To(s : out Saved_Registers_Array)
    with
        Global => (Input => Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State),
        Inline_Always;

    procedure Restore_Saved_Registers_From(s : Saved_Registers_Array)
    with
        Global => (Output => Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State),
        Inline_Always;

end Riscv_Core.Rv_Gpr;
