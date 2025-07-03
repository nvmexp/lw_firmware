-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Ada.Unchecked_Colwersion;
with Types;                use Types;
with Dev_Riscv_Csr_64;     use Dev_Riscv_Csr_64;
with Dev_Riscv_Pri;        use Dev_Riscv_Pri;
with Constants;            use Constants;
with System.Machine_Code;  use System.Machine_Code;
with Lw_Riscv_Address_Map; use Lw_Riscv_Address_Map;

package body Riscv_Core is

   procedure Ghost_Switch_To_SK_Stack is
   begin
      Ghost_Lwrrent_Stack := SK_Stack;
   end Ghost_Switch_To_SK_Stack;

   procedure Ghost_Switch_To_S_Stack is
   begin
      Ghost_Lwrrent_Stack := S_Stack;
   end Ghost_Switch_To_S_Stack;

   -------------------------------------------------------
   --  --
   -------------------------------------------------------
   package body Bar0_Reg is

      package body Bar0_Reg_Hw_Model with
         Refined_State => (Mmio_State => null),
         SPARK_Mode    => On
      is
      end Bar0_Reg_Hw_Model;

      procedure Wr32_Addr64_Mmio (Addr : LwU64; Data : LwU32) with
         SPARK_Mode => Off
      is
         Reg : LwU32 with
            Address => System'To_Address (Addr),
            Volatile;
      begin

         Reg := Data;

      end Wr32_Addr64_Mmio;

      procedure Rd32_Addr64_Mmio (Addr : LwU64; Data : out LwU32) with
         SPARK_Mode => Off
      is
         Reg : LwU32 with
            Address => System'To_Address (Addr),
            Volatile;
      begin
         Data := Reg;
      end Rd32_Addr64_Mmio;

      procedure Wr32_Addr64_Mmio_Safe (Addr : LwU64; Data : LwU32) with
         SPARK_Mode => Off
      is
         Reg : LwU32 with
            Address => System'To_Address (Addr),
            Volatile;
      begin
         Reg := Data;
         Inst.Fence_Io;
      end Wr32_Addr64_Mmio_Safe;

      procedure Rd32_Generic (Offset : LwU32; Data : out Generic_Register) is
         function Colwert_To_Reg is new Ada.Unchecked_Colwersion
           (Source => LwU32, Target => Generic_Register);

         Addr  : LwU64 := LW_RISCV_AMAP_PRIV_START + LwU64 (Offset);
         Value : LwU32;
      begin

         Rd32_Addr64_Mmio (Addr => Addr, Data => Value);
         Data := Colwert_To_Reg (Value);

      end Rd32_Generic;

      procedure Wr32_Generic (Offset : LwU32; Data : Generic_Register) is
         function Colwert_To_LwU32 is new Ada.Unchecked_Colwersion
           (Source => Generic_Register, Target => LwU32);

         Addr  : LwU64          := LW_RISCV_AMAP_PRIV_START + LwU64 (Offset);
         Value : constant LwU32 := Colwert_To_LwU32 (Data);
      begin

         Wr32_Addr64_Mmio (Addr => Addr, Data => Value);

      end Wr32_Generic;

      -------------------------------------------------------
      --  --
      -------------------------------------------------------
      package body Riscv_Reg is

         procedure Rd32_Generic (Offset : LwU32; Data : out Generic_Register)
         is
            function Colwert_To_Reg is new Ada.Unchecked_Colwersion
              (Source => LwU32, Target => Generic_Register);

            Addr : LwU64 :=
              LW_RISCV_AMAP_PRIV_START + ENGINE_BASE_RISCV + LwU64 (Offset);
            Value : LwU32;
         begin

            Rd32_Addr64_Mmio (Addr => Addr, Data => Value);
            Data := Colwert_To_Reg (Value);

         end Rd32_Generic;

         procedure Wr32_Generic (Offset : LwU32; Data : Generic_Register) is
            function Colwert_To_LwU32 is new Ada.Unchecked_Colwersion
              (Source => Generic_Register, Target => LwU32);

            Addr : LwU64 :=
              LW_RISCV_AMAP_PRIV_START + ENGINE_BASE_RISCV + LwU64 (Offset);
            Value : constant LwU32 := Colwert_To_LwU32 (Data);
         begin

            Wr32_Addr64_Mmio (Addr => Addr, Data => Value);

         end Wr32_Generic;

      end Riscv_Reg;

      -------------------------------------------------------
      --  --
      -------------------------------------------------------
      package body Flcn_Reg is

         procedure Rd32_Generic (Offset : LwU32; Data : out Generic_Register)
         is
            function Colwert_To_Reg is new Ada.Unchecked_Colwersion
              (Source => LwU32, Target => Generic_Register);

            Addr : LwU64 :=
              LW_RISCV_AMAP_PRIV_START + ENGINE_BASE_FALCON + LwU64 (Offset);
            Value : LwU32;
         begin

            Rd32_Addr64_Mmio (Addr => Addr, Data => Value);
            Data := Colwert_To_Reg (Value);

         end Rd32_Generic;

         procedure Wr32_Generic (Offset : LwU32; Data : Generic_Register) is
            function Colwert_To_LwU32 is new Ada.Unchecked_Colwersion
              (Source => Generic_Register, Target => LwU32);

            Addr : LwU64 :=
              LW_RISCV_AMAP_PRIV_START + ENGINE_BASE_FALCON + LwU64 (Offset);
            Value : constant LwU32 := Colwert_To_LwU32 (Data);
         begin

            Wr32_Addr64_Mmio (Addr => Addr, Data => Value);

         end Wr32_Generic;

      end Flcn_Reg;

      -------------------------------------------------------
      --  --
      -------------------------------------------------------
      package body Fbif_Reg is

         procedure Rd32_Generic (Offset : LwU32; Data : out Generic_Register)
         is
            function Colwert_To_Reg is new Ada.Unchecked_Colwersion
              (Source => LwU32, Target => Generic_Register);

            Addr : LwU64 :=
              LW_RISCV_AMAP_PRIV_START + ENGINE_BASE_FBIF + LwU64 (Offset);
            Value : LwU32;
         begin

            Rd32_Addr64_Mmio (Addr => Addr, Data => Value);
            Data := Colwert_To_Reg (Value);

         end Rd32_Generic;

         procedure Wr32_Generic (Offset : LwU32; Data : Generic_Register) is
            function Colwert_To_LwU32 is new Ada.Unchecked_Colwersion
              (Source => Generic_Register, Target => LwU32);

            Addr : LwU64 :=
              LW_RISCV_AMAP_PRIV_START + ENGINE_BASE_FBIF + LwU64 (Offset);
            Value : constant LwU32 := Colwert_To_LwU32 (Data);
         begin

            Wr32_Addr64_Mmio (Addr => Addr, Data => Value);

         end Wr32_Generic;

      end Fbif_Reg;

   end Bar0_Reg;

   -------------------------------------------------------
   --  --
   -------------------------------------------------------
   package body Rv_Csr is
      package body Csr_Reg_Hw_Model with
         Refined_State => (Csr_State => null),
         SPARK_Mode    => On
      is
      end Csr_Reg_Hw_Model;

      procedure Rd64_Generic (Addr : Csr_Addr; Data : out Generic_Csr) is
         function Colwert_To_Csr is new Ada.Unchecked_Colwersion
           (Source => LwU64, Target => Generic_Csr);
         Val : LwU64;
      begin
         Inst.Csrr (Addr => Addr, Rd => Val);
         Data := Colwert_To_Csr (Val);
      end Rd64_Generic;

      procedure Wr64_Generic (Addr : Csr_Addr; Data : Generic_Csr) is
         function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion
           (Source => Generic_Csr, Target => LwU64);
         Val : constant LwU64 := Colwert_To_LwU64 (Data);
      begin
         Inst.Csrw (Addr => Addr, Rs => Val);
      end Wr64_Generic;

      procedure Set64_Generic (Addr : Csr_Addr; Data : Generic_Csr) is
         function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion
           (Source => Generic_Csr, Target => LwU64);
         Val : constant LwU64 := Colwert_To_LwU64 (Data);
      begin
         Inst.Csrs (Addr => Addr, Rs => Val);

      end Set64_Generic;

      procedure Clr64_Generic (Addr : Csr_Addr; Data : Generic_Csr) is
         function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion
           (Source => Generic_Csr, Target => LwU64);
         Val : constant LwU64 := Colwert_To_LwU64 (Data);
      begin
         Inst.Csrc (Addr => Addr, Rs => Val);
      end Clr64_Generic;
   end Rv_Csr;

   procedure Halt with SPARK_Mode => Off
   is
   begin
      loop
         Inst.Csrwi
           (Addr => LW_RISCV_CSR_MOPT_Address,
            Imm  => LW_RISCV_CSR_MOPT_CMD_HALT);
      end loop;
   end Halt;

   procedure Setup_Trap_Handler (Addr : LwU64) is
   begin
      Inst.Csrw (Addr => LW_RISCV_CSR_MTVEC_Address, Rs => Addr);
   end Setup_Trap_Handler;

   procedure Setup_S_Mode_Trap_Handler (Addr : LwU64) is
   begin
      Inst.Csrw (Addr => LW_RISCV_CSR_STVEC_Address, Rs => Addr);
   end Setup_S_Mode_Trap_Handler;

   -------------------------------------------------------
   --  --
   -------------------------------------------------------
   package body Inst is

      procedure Csrrw (Addr : Csr_Addr; Rs1 : LwU64; Rd : out LwU64) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm
           ("csrrw %0, %1, %2", Outputs => LwU64'Asm_Output ("=r", Rd),
            Inputs                      =>
              (Csr_Addr'Asm_Input ("i", Addr), LwU64'Asm_Input ("r", Rs1)),
            Volatile => True);
      end Csrrw;

      procedure Csrrs (Addr : Csr_Addr; Rs1 : LwU64; Rd : out LwU64) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm
           ("csrrs %0, %1, %2", Outputs => LwU64'Asm_Output ("=r", Rd),
            Inputs                      =>
              (Csr_Addr'Asm_Input ("i", Addr), LwU64'Asm_Input ("r", Rs1)),
            Volatile => True);
      end Csrrs;

      procedure Csrrc (Addr : Csr_Addr; Rs1 : LwU64; Rd : out LwU64) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm
           ("csrrc %0, %1, %2", Outputs => LwU64'Asm_Output ("=r", Rd),
            Inputs                      =>
              (Csr_Addr'Asm_Input ("i", Addr), LwU64'Asm_Input ("r", Rs1)),
            Volatile => True);
      end Csrrc;

      procedure Csrrwi (Addr : Csr_Addr; Imm : LwU5; Rd : out LwU64) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm
           ("csrrwi %0, %1, %2", Outputs => LwU64'Asm_Output ("=r", Rd),
            Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU5'Asm_Input ("i", Imm)),
            Volatile                     => True);
      end Csrrwi;

      procedure Csrrsi (Addr : Csr_Addr; Imm : LwU5; Rd : out LwU64) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm
           ("csrrsi %0, %1, %2", Outputs => LwU64'Asm_Output ("=r", Rd),
            Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU5'Asm_Input ("i", Imm)),
            Volatile                     => True);
      end Csrrsi;

      procedure Csrrci (Addr : Csr_Addr; Imm : LwU5; Rd : out LwU64) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm
           ("csrrci %0, %1, %2", Outputs => LwU64'Asm_Output ("=r", Rd),
            Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU5'Asm_Input ("i", Imm)),
            Volatile                     => True);
      end Csrrci;

      procedure Csrr (Addr : Csr_Addr; Rd : out LwU64) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm
           ("csrr %0, %1", Outputs => LwU64'Asm_Output ("=r", Rd),
            Inputs => Csr_Addr'Asm_Input ("i", Addr), Volatile => True);
      end Csrr;

      procedure Csrw (Addr : Csr_Addr; Rs : LwU64) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm
           ("csrw %0, %1",
            Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU64'Asm_Input ("r", Rs)),
            Volatile => True);
      end Csrw;

      procedure Csrs (Addr : Csr_Addr; Rs : LwU64) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm
           ("csrs %0, %1",
            Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU64'Asm_Input ("r", Rs)),
            Volatile => True);
      end Csrs;

      procedure Csrc (Addr : Csr_Addr; Rs : LwU64) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm
           ("csrc %0, %1",
            Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU64'Asm_Input ("r", Rs)),
            Volatile => True);
      end Csrc;

      procedure Csrwi (Addr : Csr_Addr; Imm : LwU5) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm
           ("csrwi %0, %1",
            Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU5'Asm_Input ("i", Imm)),
            Volatile => True);
      end Csrwi;

      procedure Csrsi (Addr : Csr_Addr; Imm : LwU5) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm
           ("csrsi %0, %1",
            Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU5'Asm_Input ("i", Imm)),
            Volatile => True);
      end Csrsi;

      procedure Csrci (Addr : Csr_Addr; Imm : LwU5) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm
           ("csrci %0, %1",
            Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU5'Asm_Input ("i", Imm)),
            Volatile => True);
      end Csrci;

      procedure Mret with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         loop
            Asm ("mret", Volatile => True);
         end loop;
      end Mret;

      procedure Nop with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm ("nop", Volatile => True);
      end Nop;

      procedure Fence_Io with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm ("fence io,io", Volatile => True);
      end Fence_Io;

      procedure Fence_All with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm ("fence iorw,iorw", Volatile => True);
      end Fence_All;
   end Inst;

   -------------------------------------------------------
   --  --
   -------------------------------------------------------
   package body Rv_Gpr is
      package body Gpr_Reg_Hw_Model with
         Refined_State => (Gpr_State => null),
         SPARK_Mode    => On
      is
      end Gpr_Reg_Hw_Model;

      procedure Clear_Gpr with
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
--           x10 - a0
--           x11 - a1
--           x12 - a2
--           x13 - a3
--           x14 - a4
--           x15 - a5
--           x16 - a6
--           x17 - a7

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

      procedure Switch_To_Old_Stack (Load_Old_SP_Value_From : LwU32) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Ghost_Switch_To_S_Stack;

         Asm
           ("csrr sp, %0",
            Inputs   => LwU32'Asm_Input ("i", Load_Old_SP_Value_From),
            Volatile => True);
      end Switch_To_Old_Stack;

      procedure Save_Callee_Saved_Registers
        (s : out Callee_Saved_Registers) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm
           ("sd s0, %0", Outputs => LwU64'Asm_Output ("=m", s (0)),
            Volatile             => True);
         Asm
           ("sd s1, %0", Outputs => LwU64'Asm_Output ("=m", s (1)),
            Volatile             => True);
         Asm
           ("sd s2, %0", Outputs => LwU64'Asm_Output ("=m", s (2)),
            Volatile             => True);
         Asm
           ("sd s3, %0", Outputs => LwU64'Asm_Output ("=m", s (3)),
            Volatile             => True);
         Asm
           ("sd s4, %0", Outputs => LwU64'Asm_Output ("=m", s (4)),
            Volatile             => True);
         Asm
           ("sd s5, %0", Outputs => LwU64'Asm_Output ("=m", s (5)),
            Volatile             => True);
         Asm
           ("sd s6, %0", Outputs => LwU64'Asm_Output ("=m", s (6)),
            Volatile             => True);
         Asm
           ("sd s7, %0", Outputs => LwU64'Asm_Output ("=m", s (7)),
            Volatile             => True);
         Asm
           ("sd s8, %0", Outputs => LwU64'Asm_Output ("=m", s (8)),
            Volatile             => True);
         Asm
           ("sd s9, %0", Outputs => LwU64'Asm_Output ("=m", s (9)),
            Volatile             => True);
         Asm
           ("sd s10, %0", Outputs => LwU64'Asm_Output ("=m", s (10)),
            Volatile              => True);
         Asm
           ("sd s11, %0", Outputs => LwU64'Asm_Output ("=m", s (11)),
            Volatile              => True);
      end Save_Callee_Saved_Registers;

      procedure Restore_Callee_Saved_Registers
        (s : Callee_Saved_Registers) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm
           ("ld s0, %0", Inputs => LwU64'Asm_Input ("m", s (0)),
            Volatile            => True);
         Asm
           ("ld s1, %0", Inputs => LwU64'Asm_Input ("m", s (1)),
            Volatile            => True);
         Asm
           ("ld s2, %0", Inputs => LwU64'Asm_Input ("m", s (2)),
            Volatile            => True);
         Asm
           ("ld s3, %0", Inputs => LwU64'Asm_Input ("m", s (3)),
            Volatile            => True);
         Asm
           ("ld s4, %0", Inputs => LwU64'Asm_Input ("m", s (4)),
            Volatile            => True);
         Asm
           ("ld s5, %0", Inputs => LwU64'Asm_Input ("m", s (5)),
            Volatile            => True);
         Asm
           ("ld s6, %0", Inputs => LwU64'Asm_Input ("m", s (6)),
            Volatile            => True);
         Asm
           ("ld s7, %0", Inputs => LwU64'Asm_Input ("m", s (7)),
            Volatile            => True);
         Asm
           ("ld s8, %0", Inputs => LwU64'Asm_Input ("m", s (8)),
            Volatile            => True);
         Asm
           ("ld s9, %0", Inputs => LwU64'Asm_Input ("m", s (9)),
            Volatile            => True);
         Asm
           ("ld s10, %0", Inputs => LwU64'Asm_Input ("m", s (10)),
            Volatile             => True);
         Asm
           ("ld s11, %0", Inputs => LwU64'Asm_Input ("m", s (11)),
            Volatile             => True);
      end Restore_Callee_Saved_Registers;

      procedure Clear_Temporary_Registers with
         SPARK_Mode => Off --Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm ("li t0, 0", Volatile => True);
         Asm ("li t1, 0", Volatile => True);
         Asm ("li t2, 0", Volatile => True);
         Asm ("li t3, 0", Volatile => True);
         Asm ("li t4, 0", Volatile => True);
         Asm ("li t5, 0", Volatile => True);
         Asm ("li t6, 0", Volatile => True);
      end Clear_Temporary_Registers;

      procedure Load_From_Argument_Registers (a : out Argument_Registers) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin

         Asm
           ("sd a0, %0", Outputs => LwU64'Asm_Output ("=m", a (0)),
            Volatile             => True);
         Asm
           ("sd a1, %0", Outputs => LwU64'Asm_Output ("=m", a (1)),
            Volatile             => True);
         Asm
           ("sd a2, %0", Outputs => LwU64'Asm_Output ("=m", a (2)),
            Volatile             => True);
         Asm
           ("sd a3, %0", Outputs => LwU64'Asm_Output ("=m", a (3)),
            Volatile             => True);
         Asm
           ("sd a4, %0", Outputs => LwU64'Asm_Output ("=m", a (4)),
            Volatile             => True);
         Asm
           ("sd a5, %0", Outputs => LwU64'Asm_Output ("=m", a (5)),
            Volatile             => True);
         Asm
           ("sd a6, %0", Outputs => LwU64'Asm_Output ("=m", a (6)),
            Volatile             => True);
         Asm
           ("sd a7, %0", Outputs => LwU64'Asm_Output ("=m", a (7)),
            Volatile             => True);
      end Load_From_Argument_Registers;

      procedure Save_To_Argument_Registers (a : Argument_Registers) with
         SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
      is
      begin
         Asm
           ("ld a0, %0", Inputs => LwU64'Asm_Input ("m", a (0)),
            Volatile            => True);
         Asm
           ("ld a1, %0", Inputs => LwU64'Asm_Input ("m", a (1)),
            Volatile            => True);
         Asm
           ("ld a2, %0", Inputs => LwU64'Asm_Input ("m", a (2)),
            Volatile            => True);
         Asm
           ("ld a3, %0", Inputs => LwU64'Asm_Input ("m", a (3)),
            Volatile            => True);
         Asm
           ("ld a4, %0", Inputs => LwU64'Asm_Input ("m", a (4)),
            Volatile            => True);
         Asm
           ("ld a5, %0", Inputs => LwU64'Asm_Input ("m", a (5)),
            Volatile            => True);
         Asm
           ("ld a6, %0", Inputs => LwU64'Asm_Input ("m", a (6)),
            Volatile            => True);
         Asm
           ("ld a7, %0", Inputs => LwU64'Asm_Input ("m", a (7)),
            Volatile            => True);
      end Save_To_Argument_Registers;

   end Rv_Gpr;

end Riscv_Core;
