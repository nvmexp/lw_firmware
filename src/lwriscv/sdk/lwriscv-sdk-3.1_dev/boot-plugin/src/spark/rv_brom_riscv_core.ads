with Lw_Types; use Lw_Types;
with Rv_Brom_Types; use Rv_Brom_Types;

package Rv_Brom_Riscv_Core is

    package Csb_Reg is
        generic
            type Generic_Register is private;
        procedure Rd32_Generic (Addr : LwU32; Data : out Generic_Register) with
            Pre => (Addr mod 4) = 0,
            Inline_Always;

        generic
            type Generic_Register is private;
        procedure Wr32_Generic (Addr : LwU32; Data : Generic_Register) with
            Pre => (Addr mod 4) = 0,
            Inline_Always;

        procedure Wr32_Addr32_Mmio (Addr : LwU32; Data : LwU32) with
            Pre => (Addr mod 4) = 0,
            Inline_Always;

        procedure Rd32_Addr64_Mmio (Addr : LwU64; Data : out LwU32) with
            Pre => (Addr mod 4) = 0,
            Inline_Always;

        procedure Wr32_Addr64_Mmio_Safe (Addr : LwU64; Data : LwU32) with
            Pre => (Addr mod 4) = 0,
            Inline_Always;

        -------------------------------------------------------
        -- package to model the writes of hardware for spark --
        -------------------------------------------------------
        package Csb_Reg_Wr32_Hw_Model with
        Initializes    => Mmio_State,
            Abstract_State => (Mmio_State with External => (Async_Readers, Effective_Writes))
        is
            procedure Write with
                Global => (Output => Mmio_State), Inline_Always;
        end Csb_Reg_Wr32_Hw_Model;
    end Csb_Reg;


    package Rv_Csr is
        generic
            type Generic_Csr is private;
        procedure Rd64_Generic (Addr : LwU32; Data : out Generic_Csr) with Inline_Always;

        generic
            type Generic_Csr is private;
        procedure Wr64_Generic (Addr : LwU32; Data : Generic_Csr) with Inline_Always;

        -- Set corresponding bit of CSR
        generic
            type Generic_Csr is private;
        procedure Set64_Generic (Addr : LwU32; Data : Generic_Csr) with Inline_Always;

        -- Clear corresponding bit of CSR
        generic
            type Generic_Csr is private;
        procedure Clr64_Generic (Addr : LwU32; Data : Generic_Csr) with Inline_Always;

        package Csr_Reg_Wr64_Hw_Model with
        Abstract_State => (Csr_State with External => (Async_Readers, Effective_Writes))
        is
            procedure Write with
               Global => (Output => Csr_State), Inline_Always;
        end Csr_Reg_Wr64_Hw_Model;
    end Rv_Csr;

    -- Config Mspm to set the PrivLevel and Secure Mask and set lock bit to prevent further modification.
    procedure Lock_Priv_Level_And_Selwre_Mask (Selwre_Mode : HS_BOOL;
                                               Priv_Level  : LwU4) with
        Global => (Output => Rv_Csr.Csr_Reg_Wr64_Hw_Model.Csr_State),
        Inline_Always;

    -- Config Mspm and Mrsp to request the permission
    -- Set HS_TRUE to Selwre_Mode to enter Secure status for accessing the SCP
    -- Set HS_TRUE to Priv_Level3 to get PrivLevel3 or 2 for accessing external resources
    -- Set HS_FALSE to both parameters to restore to the PrivLevel0 and inselwre status
    procedure Change_Priv_Level_And_Selwre_Status (Selwre_Mode : HS_BOOL;
                                                   Priv_Level3 : HS_BOOL) with
        Global => (Output => Rv_Csr.Csr_Reg_Wr64_Hw_Model.Csr_State),
        Inline_Always;

    -- Write MSPM.MMLOCK = 1 and PL to 0
    procedure Lock_Machine_Mask with
        Global => (Output => Rv_Csr.Csr_Reg_Wr64_Hw_Model.Csr_State),
        Inline_Always;

    -- Halt the Riscv Core unconditionally, once RISCV has entered halt state,
    -- it cannot be booted up via RISCV_CPUCTL(_ALIAS).STARTCPU until reset
    procedure Halt with Inline_Always, No_Return;

    -- Setup the Trap_Handler with Addr
    procedure Setup_Trap_Handler (Addr : LwU64) with
       Global => null,
       Pre    => (Addr mod 4) = 0,
       Inline_Always;


    -- Reset used CSRs including MCYCLE, MINSTRET, MBPCFG to avoid side channel
    procedure Reset_Csrs with Global => null, Inline_Always;

    -- Set all GPRs to 0.
    pragma Warnings(Off, "subprogram ""Clear_Gpr"" has no effect", Reason => "The subprogram hasn't been implemented." &
                       " Global => null may NOT be appropriate since GPRs do have effect on program exelwtion");
    procedure Clear_Gpr with Inline_Always;
    pragma Warnings(On, "subprogram ""Clear_Gpr"" has no effect");

    procedure Set_Mepc (Addr : LwU64) with Global => null, Inline_Always;

    -- Unconditionally remove PL0 access to FALCON_CPUCTL.SRESET/HRESET by programming FALCON_CPUCTL_PRIV_LEVEL_MASK.
    procedure Remove_Pl0_Access_For_Cpuctl with
        Global => (Output => Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

    procedure Scrub_Imem_Block (Start : Imem_Offset_Byte; Size : Imem_Size_Byte) with
        Pre => (Start mod 256 = 0) and then (Size mod 256 = 0),
        Global => null,
        Inline_Always;

    pragma Warnings (Off, "subprogram ""Scrub_Dmem_Block"" has no effect", Reason => "The subprogram hasn't been implemented." &
                         " Global => null may NOT be appropriate since DMEM change may have an effect on program exelwtion");
    -- It's better to change the parameters type to Dmem_Offset/Size_Block, but that will be difflwlt for the caller to use.
    -- We check the parameters in precondition.
    procedure Scrub_Dmem_Block (Start : Dmem_Offset_Byte; Size : Dmem_Size_Byte) with
        Pre => (Start mod 256 = 0) and then (Size mod 256 = 0),
        Inline_Always;
    pragma Warnings(On, "subprogram ""Scrub_Dmem_Block"" has no effect");


    package Inst is
        -- The CSRRW (Atomic Read/Write CSR) instruction atomically swaps values in the CSRs and
        -- integer registers. CSRRW reads the old value of the CSR, zero-extends the value to XLEN bits,
        -- then writes it to integer register rd. The initial value in rs1 is written to the CSR. If rd=x0, then
        -- the instruction shall not read the CSR and shall not cause any of the side-effects that might occur
        -- on a CSR read.
        procedure Csrrw (Addr : LwU32; Rs1 : LwU64; Rd : out LwU64) with Inline_Always;

        -- The CSRRS (Atomic Read and Set Bits in CSR) instruction reads the value of the CSR, zero-
        -- extends the value to XLEN bits, and writes it to integer register rd. The initial value in integer
        -- register rs1 is treated as a bit mask that specifies bit positions to be set in the CSR. Any bit that
        -- is high in rs1 will cause the corresponding bit to be set in the CSR, if that CSR bit is writable.
        -- Other bits in the CSR are unaffected (though CSRs might have side effects when written).
        procedure Csrrs (Addr : LwU32; Rs1 : LwU64; Rd : out LwU64) with Inline_Always;

        -- The CSRRC (Atomic Read and Clear Bits in CSR) instruction reads the value of the CSR, zero-
        -- extends the value to XLEN bits, and writes it to integer register rd. The initial value in integer
        -- register rs1 is treated as a bit mask that specifies bit positions to be cleared in the CSR. Any bit
        -- that is high in rs1 will cause the corresponding bit to be cleared in the CSR, if that CSR bit is
        -- writable. Other bits in the CSR are unaffected
        procedure Csrrc (Addr : LwU32; Rs1 : LwU64; Rd : out LwU64) with Inline_Always;

        -- The CSRRWI, CSRRSI, and CSRRCI variants are similar to CSRRW, CSRRS, and CSRRC re-
        -- spectively, except they update the CSR using an XLEN-bit value obtained by zero-extending a 5-bit
        -- unsigned immediate (uimm[4:0]) field encoded in the rs1 field instead of a value from an integer.
        procedure Csrrwi (Addr : LwU32; Imm : LwU5; Rd : out LwU64) with Inline_Always;
        procedure Csrrsi (Addr : LwU32; Imm : LwU5; Rd : out LwU64) with Inline_Always;
        procedure Csrrci (Addr : LwU32; Imm : LwU5; Rd : out LwU64) with Inline_Always;

        -- Simple form
        -- Read Csr to rd, encoded as csrrs rd, csr, x0
        procedure Csrr (Addr : LwU32; Rd : out LwU64) with Inline_Always;
        -- Write rs to Csr, encoded as csrrw x0, csr, rs
        procedure Csrw (Addr : LwU32; Rs : LwU64) with Inline_Always;
        -- Set bits in Csr, encoded as csrrs x0, csr, rs
        procedure Csrs (Addr : LwU32; Rs : LwU64) with Inline_Always;
        -- Clear bits in Csr, encoded as csrrc x0, csr, rs
        procedure Csrc (Addr : LwU32; Rs : LwU64) with Inline_Always;
        -- Write Immediate to Csr, encoded as csrrwi x0, csr, imm
        procedure Csrwi (Addr : LwU32; Imm : LwU5) with Inline_Always;
        -- Set Immediate in Csr, encoded as csrrsi, x0, csr, imm
        procedure Csrsi (Addr : LwU32; Imm : LwU5) with Inline_Always;
        -- Clear Immediate in Csr, encoded as csrrci x0, csr, imm
        procedure Csrci (Addr : LwU32; Imm : LwU5) with Inline_Always;

        -- Environment Call
        procedure Ecall with Inline_Always;

        procedure Nop with Inline_Always;

        -- Fence.IO instruction to make sure IO operation is done
        procedure Fence_Io with Inline_Always;

        -- Light weight fence.All instruction to make sure IO and memory operation are all done
        -- Compared to Fence.all, this one won't issue SYSOP to FB interface.
        procedure Lwfence_All with Inline_Always;

    end Inst;

end Rv_Brom_Riscv_Core;
