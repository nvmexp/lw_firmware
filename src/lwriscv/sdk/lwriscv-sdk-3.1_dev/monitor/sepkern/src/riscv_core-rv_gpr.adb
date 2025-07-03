-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with System.Machine_Code; use System.Machine_Code;

package body Riscv_Core.Rv_Gpr is

    -- Gpr_Reg_Hw_Model package
    package body Gpr_Reg_Hw_Model
    with
        Refined_State => (Gpr_State => null),
        SPARK_Mode    => On
    is
    end Gpr_Reg_Hw_Model;
    -----------------------------------------------------------------------

    procedure Clear_Gpr
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm ("li x1, 0", Volatile => True);
        Asm ("li x2, 0", Volatile => True);
        Asm ("li x3, 0", Volatile => True);
        Asm ("li x4, 0", Volatile => True);
        Asm ("li x5, 0", Volatile => True);
        Asm ("li x6, 0", Volatile => True);
        Asm ("li x7, 0", Volatile => True);
        Asm ("li x8, 0", Volatile => True);
        Asm ("li x9, 0", Volatile => True);

        -- Don't clear argument registers
        -- x10 - a0
        -- x11 - a1
        -- x12 - a2
        -- x13 - a3
        -- x14 - a4
        -- x15 - a5
        -- x16 - a6
        -- x17 - a7

        Asm ("li x18, 0", Volatile => True);
        Asm ("li x19, 0", Volatile => True);
        Asm ("li x20, 0", Volatile => True);
        Asm ("li x21, 0", Volatile => True);
        Asm ("li x22, 0", Volatile => True);
        Asm ("li x23, 0", Volatile => True);
        Asm ("li x24, 0", Volatile => True);
        Asm ("li x25, 0", Volatile => True);
        Asm ("li x26, 0", Volatile => True);
        Asm ("li x27, 0", Volatile => True);
        Asm ("li x28, 0", Volatile => True);
        Asm ("li x29, 0", Volatile => True);
        Asm ("li x30, 0", Volatile => True);
        Asm ("li x31, 0", Volatile => True);
    end Clear_Gpr;

    procedure Switch_To_Old_Stack (Load_Old_SP_Value_From : LwU32)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Ghost_Switch_To_S_Stack;

        Asm
        ("csrr sp, %0", Inputs   => LwU32'Asm_Input ("i", Load_Old_SP_Value_From),
                        Volatile => True);

    end Switch_To_Old_Stack;

    procedure Save_Pointer_Registers_To(p : out Pointer_Registers_Array)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("sd ra, %0", Outputs  => LwU64'Asm_Output ("=m", p (0)),
                      Volatile => True);
        Asm
        ("sd gp, %0", Outputs  => LwU64'Asm_Output ("=m", p (1)),
                      Volatile => True);
        Asm
        ("sd tp, %0", Outputs  => LwU64'Asm_Output ("=m", p (2)),
                      Volatile => True);
        Asm
        ("sd fp, %0", Outputs  => LwU64'Asm_Output ("=m", p (3)),
                      Volatile => True);

    end Save_Pointer_Registers_To;

    procedure Restore_Pointer_Registers_From(p : Pointer_Registers_Array)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("ld ra, %0", Inputs   => LwU64'Asm_Input ("m", p (0)),
                      Volatile => True);
        Asm
        ("ld gp, %0", Inputs   => LwU64'Asm_Input ("m", p (1)),
                      Volatile => True);
        Asm
        ("ld tp, %0", Inputs   => LwU64'Asm_Input ("m", p (2)),
                      Volatile => True);
        Asm
        ("ld fp, %0", Inputs   => LwU64'Asm_Input ("m", p (3)),
                      Volatile => True);

    end Restore_Pointer_Registers_From;

    procedure Save_Temp_Registers_To(t : out Temp_Registers_Array)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("sd t0, %0", Outputs  => LwU64'Asm_Output ("=m", t (0)),
                      Volatile => True);
        Asm
        ("sd t1, %0", Outputs  => LwU64'Asm_Output ("=m", t (1)),
                      Volatile => True);
        Asm
        ("sd t2, %0", Outputs  => LwU64'Asm_Output ("=m", t (2)),
                      Volatile => True);
        Asm
        ("sd t3, %0", Outputs  => LwU64'Asm_Output ("=m", t (3)),
                      Volatile => True);
        Asm
        ("sd t4, %0", Outputs  => LwU64'Asm_Output ("=m", t (4)),
                      Volatile => True);
        Asm
        ("sd t5, %0", Outputs  => LwU64'Asm_Output ("=m", t (5)),
                      Volatile => True);
        Asm
        ("sd t6, %0", Outputs  => LwU64'Asm_Output ("=m", t (6)),
                      Volatile => True);

    end Save_Temp_Registers_To;

    procedure Restore_Temp_Registers_From(t : Temp_Registers_Array)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("ld t0, %0", Inputs   => LwU64'Asm_Input ("m", t (0)),
                      Volatile => True);
        Asm
        ("ld t1, %0", Inputs   => LwU64'Asm_Input ("m", t (1)),
                      Volatile => True);
        Asm
        ("ld t2, %0", Inputs   => LwU64'Asm_Input ("m", t (2)),
                      Volatile => True);
        Asm
        ("ld t3, %0", Inputs   => LwU64'Asm_Input ("m", t (3)),
                      Volatile => True);
        Asm
        ("ld t4, %0", Inputs   => LwU64'Asm_Input ("m", t (4)),
                      Volatile => True);
        Asm
        ("ld t5, %0", Inputs   => LwU64'Asm_Input ("m", t (5)),
                      Volatile => True);
        Asm
        ("ld t6, %0", Inputs   => LwU64'Asm_Input ("m", t (6)),
                      Volatile => True);

    end Restore_Temp_Registers_From;

    procedure Save_Argument_Registers_To (a : out Argument_Registers_Array)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("sd a0, %0", Outputs  => LwU64'Asm_Output ("=m", a (0)),
                      Volatile => True);
        Asm
        ("sd a1, %0", Outputs  => LwU64'Asm_Output ("=m", a (1)),
                      Volatile => True);
        Asm
        ("sd a2, %0", Outputs  => LwU64'Asm_Output ("=m", a (2)),
                      Volatile => True);
        Asm
        ("sd a3, %0", Outputs  => LwU64'Asm_Output ("=m", a (3)),
                      Volatile => True);
        Asm
        ("sd a4, %0", Outputs  => LwU64'Asm_Output ("=m", a (4)),
                      Volatile => True);
        Asm
        ("sd a5, %0", Outputs  => LwU64'Asm_Output ("=m", a (5)),
                      Volatile => True);
        Asm
        ("sd a6, %0", Outputs  => LwU64'Asm_Output ("=m", a (6)),
                      Volatile => True);
        Asm
        ("sd a7, %0", Outputs  => LwU64'Asm_Output ("=m", a (7)),
                      Volatile => True);

    end Save_Argument_Registers_To;

    procedure Restore_Argument_Registers_From (a : Argument_Registers_Array)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("ld a0, %0", Inputs   => LwU64'Asm_Input ("m", a (0)),
                      Volatile => True);
        Asm
        ("ld a1, %0", Inputs   => LwU64'Asm_Input ("m", a (1)),
                      Volatile => True);
        Asm
        ("ld a2, %0", Inputs   => LwU64'Asm_Input ("m", a (2)),
                      Volatile => True);
        Asm
        ("ld a3, %0", Inputs   => LwU64'Asm_Input ("m", a (3)),
                      Volatile => True);
        Asm
        ("ld a4, %0", Inputs   => LwU64'Asm_Input ("m", a (4)),
                      Volatile => True);
        Asm
        ("ld a5, %0", Inputs   => LwU64'Asm_Input ("m", a (5)),
                      Volatile => True);
        Asm
        ("ld a6, %0", Inputs   => LwU64'Asm_Input ("m", a (6)),
                      Volatile => True);
        Asm
        ("ld a7, %0", Inputs   => LwU64'Asm_Input ("m", a (7)),
                      Volatile => True);

    end Restore_Argument_Registers_From;

    procedure Save_Saved_Registers_To (s : out Saved_Registers_Array)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("sd s1, %0", Outputs  => LwU64'Asm_Output ("=m", s (0)),
                      Volatile => True);
        Asm
        ("sd s2, %0", Outputs  => LwU64'Asm_Output ("=m", s (1)),
                      Volatile => True);
        Asm
        ("sd s3, %0", Outputs  => LwU64'Asm_Output ("=m", s (2)),
                      Volatile => True);
        Asm
        ("sd s4, %0", Outputs  => LwU64'Asm_Output ("=m", s (3)),
                      Volatile => True);
        Asm
        ("sd s5, %0", Outputs  => LwU64'Asm_Output ("=m", s (4)),
                      Volatile => True);
        Asm
        ("sd s6, %0", Outputs  => LwU64'Asm_Output ("=m", s (5)),
                      Volatile => True);
        Asm
        ("sd s7, %0", Outputs  => LwU64'Asm_Output ("=m", s (6)),
                      Volatile => True);
        Asm
        ("sd s8, %0", Outputs  => LwU64'Asm_Output ("=m", s (7)),
                      Volatile => True);
        Asm
        ("sd s9, %0", Outputs  => LwU64'Asm_Output ("=m", s (8)),
                      Volatile => True);
        Asm
        ("sd s10, %0", Outputs  => LwU64'Asm_Output ("=m", s (9)),
                       Volatile => True);
        Asm
        ("sd s11, %0", Outputs  => LwU64'Asm_Output ("=m", s (10)),
                       Volatile => True);

    end Save_Saved_Registers_To;

    procedure Restore_Saved_Registers_From (s : Saved_Registers_Array)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("ld s1, %0", Inputs   => LwU64'Asm_Input ("m", s (0)),
                      Volatile => True);
        Asm
        ("ld s2, %0", Inputs   => LwU64'Asm_Input ("m", s (1)),
                      Volatile => True);
        Asm
        ("ld s3, %0", Inputs   => LwU64'Asm_Input ("m", s (2)),
                      Volatile => True);
        Asm
        ("ld s4, %0", Inputs   => LwU64'Asm_Input ("m", s (3)),
                      Volatile => True);
        Asm
        ("ld s5, %0", Inputs   => LwU64'Asm_Input ("m", s (4)),
                      Volatile => True);
        Asm
        ("ld s6, %0", Inputs   => LwU64'Asm_Input ("m", s (5)),
                      Volatile => True);
        Asm
        ("ld s7, %0", Inputs   => LwU64'Asm_Input ("m", s (6)),
                      Volatile => True);
        Asm
        ("ld s8, %0", Inputs   => LwU64'Asm_Input ("m", s (7)),
                      Volatile => True);
        Asm
        ("ld s9, %0", Inputs   => LwU64'Asm_Input ("m", s (8)),
                      Volatile => True);
        Asm
        ("ld s10, %0", Inputs   => LwU64'Asm_Input ("m", s (9)),
                       Volatile => True);
        Asm
        ("ld s11, %0", Inputs   => LwU64'Asm_Input ("m", s (10)),
                       Volatile => True);

    end Restore_Saved_Registers_From;

end Riscv_Core.Rv_Gpr;