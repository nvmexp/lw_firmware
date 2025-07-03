-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Types; use Types;

package Riscv_Core is

   type Stacks is (SK_Stack, S_Stack, Zero_SP) with Ghost;
   Ghost_Lwrrent_Stack :  Stacks with Ghost;

   procedure Ghost_Switch_To_SK_Stack with 
     Ghost,
     Post => Ghost_Lwrrent_Stack = SK_Stack;

   procedure Ghost_Switch_To_S_Stack with 
     Ghost,
     Post => Ghost_Lwrrent_Stack = S_Stack;

   package Bar0_Reg is

      -------------------------------------------------------
      -- package to model the writes of hardware for spark --
      -------------------------------------------------------
      package Bar0_Reg_Hw_Model with
      Initializes    => Mmio_State,
        Abstract_State => (Mmio_State with External => (Async_Readers, Effective_Writes))
      is
      end Bar0_Reg_Hw_Model;

      procedure Rd32_Addr64_Mmio (Addr : LwU64; Data : out LwU32) with
        Pre => (Addr mod 4) = 0,
        Global => (Input => Bar0_Reg_Hw_Model.Mmio_State),
        Inline_Always;

      procedure Wr32_Addr64_Mmio (Addr : LwU64; Data : LwU32) with
        Pre => (Addr mod 4) = 0,
        Global => (Output => Bar0_Reg_Hw_Model.Mmio_State),
        Inline_Always;
      
      procedure Wr32_Addr64_Mmio_Safe (Addr : LwU64; Data : LwU32) with
        Pre => (Addr mod 4) = 0,
        Global => (Output => Bar0_Reg_Hw_Model.Mmio_State),
        Inline_Always;

      generic
         type Generic_Register is private;
      procedure Rd32_Generic (Offset : LwU32; Data : out Generic_Register) with
        Pre => (Offset mod 4) = 0,
        Global => (Input => Bar0_Reg_Hw_Model.Mmio_State),
        Inline_Always;

      generic
         type Generic_Register is private;
      procedure Wr32_Generic (Offset : LwU32; Data : Generic_Register) with
        Pre => (Offset mod 4) = 0,
        Global => (Output => Bar0_Reg_Hw_Model.Mmio_State),
        Inline_Always;

      -------------------------------------------------------
      --  --
      -------------------------------------------------------      
      package Riscv_Reg is
         generic
         type Generic_Register is private;
      procedure Rd32_Generic (Offset : LwU32; Data : out Generic_Register) with
           Pre => (Offset mod 4) = 0,
           Global => (Input => Bar0_Reg_Hw_Model.Mmio_State),
           Inline_Always;

         generic
         type Generic_Register is private;
      procedure Wr32_Generic (Offset : LwU32; Data : Generic_Register) with
           Pre => (Offset mod 4) = 0,
           Global => (Output => Bar0_Reg_Hw_Model.Mmio_State),
           Inline_Always;
      end Riscv_Reg;

      -------------------------------------------------------
      --  --
      -------------------------------------------------------
      package Flcn_Reg is
         generic
         type Generic_Register is private;
      procedure Rd32_Generic (Offset : LwU32; Data : out Generic_Register) with
           Pre => (Offset mod 4) = 0,
           Global => (Input => Bar0_Reg_Hw_Model.Mmio_State),
           Inline_Always;

         generic
         type Generic_Register is private;
      procedure Wr32_Generic (Offset : LwU32; Data : Generic_Register) with
           Pre => (Offset mod 4) = 0,
           Global => (Output => Bar0_Reg_Hw_Model.Mmio_State),
           Inline_Always;
      end Flcn_Reg;

      -------------------------------------------------------
      --  --
      -------------------------------------------------------
      package Fbif_Reg is
         generic
         type Generic_Register is private;
      procedure Rd32_Generic (Offset : LwU32; Data : out Generic_Register) with
        Pre => (Offset mod 4) = 0,
        Inline_Always;

         generic
         type Generic_Register is private;
      procedure Wr32_Generic (Offset : LwU32; Data : Generic_Register) with
        Pre => (Offset mod 4) = 0,
        Inline_Always;
      end Fbif_Reg;

   end Bar0_Reg;


   package Rv_Csr is

      package Csr_Reg_Hw_Model with
      Abstract_State => (Csr_State with External => (Async_Readers, Effective_Writes))
      is
      end Csr_Reg_Hw_Model;

      generic
         type Generic_Csr is private;
      procedure Rd64_Generic (Addr : Csr_Addr; Data : out Generic_Csr) with 
        Global => (Input => Csr_Reg_Hw_Model.Csr_State),
        Inline_Always;

      generic
         type Generic_Csr is private;
      procedure Wr64_Generic (Addr : Csr_Addr; Data : Generic_Csr) with 
        Global => (Output => Csr_Reg_Hw_Model.Csr_State),
        Inline_Always;

      -- Set corresponding bit of CSR
      generic
         type Generic_Csr is private;
      procedure Set64_Generic (Addr : Csr_Addr; Data : Generic_Csr) with 
        Global => (Output => Csr_Reg_Hw_Model.Csr_State),
        Inline_Always;

      -- Clear corresponding bit of CSR
      generic
         type Generic_Csr is private;
      procedure Clr64_Generic (Addr : Csr_Addr; Data : Generic_Csr) with 
        Global => (Output => Csr_Reg_Hw_Model.Csr_State),
        Inline_Always;

   end Rv_Csr;


   -- Halt the Riscv Core unconditionally, once RISCV has entered halt state,
   -- it cannot be booted up via RISCV_CPUCTL(_ALIAS).STARTCPU until reset
   procedure Halt with 
     Global => (Output => Rv_Csr.Csr_Reg_Hw_Model.Csr_State),
     Inline_Always, 
     No_Return;

   -- Setup the Trap_Handler with Addr
   procedure Setup_Trap_Handler (Addr : LwU64) with
     Global => (Output => Rv_Csr.Csr_Reg_Hw_Model.Csr_State),
     Pre    => (Addr mod 4) = 0,
     Inline_Always;

   -- Setup the Trap_Handler with Addr
   procedure Setup_S_Mode_Trap_Handler (Addr : LwU64) with
     Global => (Output => Rv_Csr.Csr_Reg_Hw_Model.Csr_State),
     Pre    => (Addr mod 4) = 0,
     Inline_Always;

   package Inst is
      -- The CSRRW (Atomic Read/Write CSR) instruction atomically swaps values in the CSRs and
      -- integer registers. CSRRW reads the old value of the CSR, zero-extends the value to XLEN bits,
      -- then writes it to integer register rd. The initial value in rs1 is written to the CSR. If rd=x0, then
      -- the instruction shall not read the CSR and shall not cause any of the side-effects that might occur
      -- on a CSR read.
      procedure Csrrw (Addr : Csr_Addr; Rs1 : LwU64; Rd : out LwU64) with 
        Inline_Always,
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);


      -- The CSRRS (Atomic Read and Set Bits in CSR) instruction reads the value of the CSR, zero-
      -- extends the value to XLEN bits, and writes it to integer register rd. The initial value in integer
      -- register rs1 is treated as a bit mask that specifies bit positions to be set in the CSR. Any bit that
      -- is high in rs1 will cause the corresponding bit to be set in the CSR, if that CSR bit is writable.
      -- Other bits in the CSR are unaffected (though CSRs might have side effects when written).
      procedure Csrrs (Addr : Csr_Addr; Rs1 : LwU64; Rd : out LwU64) with 
        Inline_Always,
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);          

      -- The CSRRC (Atomic Read and Clear Bits in CSR) instruction reads the value of the CSR, zero-
      -- extends the value to XLEN bits, and writes it to integer register rd. The initial value in integer
      -- register rs1 is treated as a bit mask that specifies bit positions to be cleared in the CSR. Any bit
      -- that is high in rs1 will cause the corresponding bit to be cleared in the CSR, if that CSR bit is
      -- writable. Other bits in the CSR are unaffected
      procedure Csrrc (Addr : Csr_Addr; Rs1 : LwU64; Rd : out LwU64) with Inline_Always;

      -- The CSRRWI, CSRRSI, and CSRRCI variants are similar to CSRRW, CSRRS, and CSRRC re-
      -- spectively, except they update the CSR using an XLEN-bit value obtained by zero-extending a 5-bit
      -- unsigned immediate (uimm[4:0]) field encoded in the rs1 field instead of a value from an integer.
      procedure Csrrwi (Addr : Csr_Addr; Imm : LwU5; Rd : out LwU64) with Inline_Always;
      procedure Csrrsi (Addr : Csr_Addr; Imm : LwU5; Rd : out LwU64) with Inline_Always;
      procedure Csrrci (Addr : Csr_Addr; Imm : LwU5; Rd : out LwU64) with Inline_Always;

      -- Simple form
      -- Read Csr to rd, encoded as csrrs rd, csr, x0
      procedure Csrr (Addr : Csr_Addr; Rd : out LwU64) with 
        Inline_Always,
        Global => (Input => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);
      
      -- Write rs to Csr, encoded as csrrw x0, csr, rs
      procedure Csrw (Addr : Csr_Addr; Rs : LwU64) with 
        Inline_Always,
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

      -- Set bits in Csr, encoded as csrrs x0, csr, rs
      procedure Csrs (Addr : Csr_Addr; Rs : LwU64) with 
        Inline_Always,
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

      -- Clear bits in Csr, encoded as csrrc x0, csr, rs
      procedure Csrc (Addr : Csr_Addr; Rs : LwU64) with 
        Inline_Always,
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

      -- Write Immediate to Csr, encoded as csrrwi x0, csr, imm
      procedure Csrwi (Addr : Csr_Addr; Imm : LwU5) with 
        Inline_Always,
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

      -- Set Immediate in Csr, encoded as csrrsi, x0, csr, imm
      procedure Csrsi (Addr : Csr_Addr; Imm : LwU5) with 
        Inline_Always,
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

      -- Clear Immediate in Csr, encoded as csrrci x0, csr, imm
      procedure Csrci (Addr : Csr_Addr; Imm : LwU5) with 
        Inline_Always,
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

      -- Transfer exelwtion to S mode    
      procedure Mret with 
        Pre => (Ghost_Lwrrent_Stack = S_Stack or else Ghost_Lwrrent_Stack = Zero_SP),
        Inline_Always, 
        No_Return;

      procedure Nop with Inline_Always;

      -- Fence.IO instruction to make sure IO operation is done
      procedure Fence_Io with Inline_Always;

      -- Fence.All instruction to make sure IO and memory operation are all done
      procedure Fence_All with Inline_Always;

   end Inst;

   package Rv_Gpr is
      package Gpr_Reg_Hw_Model with
      Abstract_State => (Gpr_State with External => (Async_Readers, Effective_Writes))
      is
      end Gpr_Reg_Hw_Model;


      -- Set all GPRs to 0.
      procedure Clear_Gpr with 
        Post => (Riscv_Core.Ghost_Lwrrent_Stack = Zero_SP),
        Global => (Output => (Gpr_Reg_Hw_Model.Gpr_State, Riscv_Core.Ghost_Lwrrent_Stack)),
        Inline_Always;

      procedure Switch_To_Old_Stack (Load_Old_SP_Value_From : LwU32)  with 
        Post => Riscv_Core.Ghost_Lwrrent_Stack = S_Stack,
        Inline_Always;
      procedure Save_Callee_Saved_Registers(s : out Callee_Saved_Registers) with Inline_Always;
      procedure Restore_Callee_Saved_Registers(s : Callee_Saved_Registers) with Inline_Always;
      procedure Load_From_Argument_Registers(a : out Argument_Registers) with Inline_Always;

      procedure Save_To_Argument_Registers(a : Argument_Registers) with 
        Global => (Output => Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State), 
        Inline_Always;

   end Rv_Gpr;
end Riscv_Core;
