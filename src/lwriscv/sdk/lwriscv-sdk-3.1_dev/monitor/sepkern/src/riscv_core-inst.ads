-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core.Rv_Csr;

package Riscv_Core.Inst
is
    -- The CSRRW (Atomic Read/Write CSR) instruction atomically swaps values in the CSRs and
    -- integer registers. CSRRW reads the old value of the CSR, zero-extends the value to XLEN bits,
    -- then writes it to integer register rd. The initial value in rs1 is written to the CSR. If rd=x0, then
    -- the instruction shall not read the CSR and shall not cause any of the side-effects that might occur
    -- on a CSR read.
    procedure Csrrw (Addr : Csr_Addr; Rs1 : LwU64; Rd : out LwU64)
    with
        Inline_Always,
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

    -- The CSRRS (Atomic Read and Set Bits in CSR) instruction reads the value of the CSR, zero-
    -- extends the value to XLEN bits, and writes it to integer register rd. The initial value in integer
    -- register rs1 is treated as a bit mask that specifies bit positions to be set in the CSR. Any bit that
    -- is high in rs1 will cause the corresponding bit to be set in the CSR, if that CSR bit is writable.
    -- Other bits in the CSR are unaffected (though CSRs might have side effects when written).
    procedure Csrrs (Addr : Csr_Addr; Rs1 : LwU64; Rd : out LwU64)
    with
        Inline_Always,
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

    -- The CSRRC (Atomic Read and Clear Bits in CSR) instruction reads the value of the CSR, zero-
    -- extends the value to XLEN bits, and writes it to integer register rd. The initial value in integer
    -- register rs1 is treated as a bit mask that specifies bit positions to be cleared in the CSR. Any bit
    -- that is high in rs1 will cause the corresponding bit to be cleared in the CSR, if that CSR bit is
    -- writable. Other bits in the CSR are unaffected
    procedure Csrrc (Addr : Csr_Addr; Rs1 : LwU64; Rd : out LwU64)
    with
        Inline_Always;

    -- The CSRRWI, CSRRSI, and CSRRCI variants are similar to CSRRW, CSRRS, and CSRRC re-
    -- spectively, except they update the CSR using an XLEN-bit value obtained by zero-extending a 5-bit
    -- unsigned immediate (uimm[4:0]) field encoded in the rs1 field instead of a value from an integer.
    procedure Csrrwi (Addr : Csr_Addr; Imm : LwU5; Rd : out LwU64)
    with
        Inline_Always;

    procedure Csrrsi (Addr : Csr_Addr; Imm : LwU5; Rd : out LwU64)
    with
        Inline_Always;

    procedure Csrrci (Addr : Csr_Addr; Imm : LwU5; Rd : out LwU64)
    with
        Inline_Always;

    -- Simple form
    -- Read Csr to rd, encoded as csrrs rd, csr, x0
    procedure Csrr (Addr : Csr_Addr; Rd : out LwU64)
    with
        Inline_Always,
        Global => (Input => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

    -- Write rs to Csr, encoded as csrrw x0, csr, rs
    procedure Csrw (Addr : Csr_Addr; Rs : LwU64)
    with
        Inline_Always,
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

    -- Set bits in Csr, encoded as csrrs x0, csr, rs
    procedure Csrs (Addr : Csr_Addr; Rs : LwU64)
    with
        Inline_Always,
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

    -- Clear bits in Csr, encoded as csrrc x0, csr, rs
    procedure Csrc (Addr : Csr_Addr; Rs : LwU64)
    with
        Inline_Always,
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

    -- Write Immediate to Csr, encoded as csrrwi x0, csr, imm
    procedure Csrwi (Addr : Csr_Addr; Imm : LwU5)
    with
        Inline_Always,
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

    -- Set Immediate in Csr, encoded as csrrsi, x0, csr, imm
    procedure Csrsi (Addr : Csr_Addr; Imm : LwU5)
    with
        Inline_Always,
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

    -- Clear Immediate in Csr, encoded as csrrci x0, csr, imm
    procedure Csrci (Addr : Csr_Addr; Imm : LwU5)
    with
        Inline_Always,
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

    -- Transfer exelwtion to S mode
    procedure Mret
    with
        Pre => (Ghost_Lwrrent_Stack = S_Stack or else Ghost_Lwrrent_Stack = Zero_SP),
        Inline_Always,
        No_Return;

    procedure Nop
    with
        Inline_Always;

    -- Fence.IO instruction to make sure IO operation is done
    procedure Fence_Io
    with
        Inline_Always;

    -- Fence.All instruction to make sure IO and memory operation are all done
    procedure Fence_All
    with
        Inline_Always;

    -- SFence.VMA instruction to re-sync address translation and access protection
    procedure Fence_VMA
    with
        Inline_Always;

    -- Halt the Riscv Core unconditionally
    procedure Halt
    with
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State),
        Inline_Always,
        No_Return;

end Riscv_Core.Inst;
