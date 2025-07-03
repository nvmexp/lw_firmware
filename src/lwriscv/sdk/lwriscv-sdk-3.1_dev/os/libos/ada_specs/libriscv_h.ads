pragma Ada_2012;
pragma Style_Checks (Off);

with Interfaces.C; use Interfaces.C;
with lwtypes_h;

package libriscv_h is

  -- _LWRM_COPYRIGHT_BEGIN_
  -- *
  -- * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
  -- * information contained herein is proprietary and confidential to LWPU
  -- * Corporation.  Any use, reproduction, or disclosure without the written
  -- * permission of LWPU Corporation is prohibited.
  -- *
  -- * _LWRM_COPYRIGHT_END_
  --  

   type LibosRiscvRegister is 
     (RISCV_X0,
      RISCV_RA,
      RISCV_SP,
      RISCV_GP,
      RISCV_TP,
      RISCV_T0,
      RISCV_T1,
      RISCV_T2,
      RISCV_S0,
      RISCV_S1,
      RISCV_A0,
      RISCV_A1,
      RISCV_A2,
      RISCV_A3,
      RISCV_A4,
      RISCV_A5,
      RISCV_A6,
      RISCV_A7,
      RISCV_S2,
      RISCV_S3,
      RISCV_S4,
      RISCV_S5,
      RISCV_S6,
      RISCV_S7,
      RISCV_S8,
      RISCV_S9,
      RISCV_S10,
      RISCV_S11,
      RISCV_T3,
      RISCV_T4,
      RISCV_T5,
      RISCV_T6)
   with Convention => C;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libriscv.h:49

   type LibosRiscvOpcode is 
     (RISCV_LB,
      RISCV_LH,
      RISCV_LW,
      RISCV_LD,
      RISCV_LBU,
      RISCV_LHU,
      RISCV_LWU,
      RISCV_SB,
      RISCV_SH,
      RISCV_SW,
      RISCV_SD,
      RISCV_LI,
      RISCV_LUI,
      RISCV_AUIPC,
      RISCV_SRLI,
      RISCV_SRLIW,
      RISCV_SRAI,
      RISCV_SRAIW,
      RISCV_SLLW,
      RISCV_DIV,
      RISCV_DIVW,
      RISCV_DIVU,
      RISCV_DIVUW,
      RISCV_REM,
      RISCV_REMU,
      RISCV_SRLW,
      RISCV_SRAW,
      RISCV_SLL,
      RISCV_SRL,
      RISCV_SRA,
      RISCV_SLLI,
      RISCV_SLLIW,
      RISCV_SLT,
      RISCV_SLTU,
      RISCV_SLTI,
      RISCV_SLTIU,
      RISCV_XORI,
      RISCV_ORI,
      RISCV_SUB,
      RISCV_SUBW,
      RISCV_ADDW,
      RISCV_MULW,
      RISCV_MUL,
      RISCV_XOR,
      RISCV_OR,
      RISCV_AND,
      RISCV_ADD,
      RISCV_ADDI,
      RISCV_ADDIW,
      RISCV_ANDI,
      RISCV_ANDIW,
      RISCV_JAL,
      RISCV_JALR,
      RISCV_BEQ,
      RISCV_BNE,
      RISCV_BLT,
      RISCV_BGE,
      RISCV_BLTU,
      RISCV_BGEU,
      RISCV_EBREAK,
      RISCV_MRET,
      RISCV_SRET,
      RISCV_WFI,
      RISCV_ECALL,
      RISCV_CSRRW,
      RISCV_CSRRS,
      RISCV_CSRRC,
      RISCV_CSRRWI,
      RISCV_CSRRSI,
      RISCV_CSRRCI,
      RISCV_REMW,
      RISCV_REMUW)
   with Convention => C;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libriscv.h:124

   type LibosRiscvInstruction is record
      opcode : aliased LibosRiscvOpcode;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libriscv.h:127
      rs1 : aliased LibosRiscvRegister;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libriscv.h:128
      rs2 : aliased LibosRiscvRegister;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libriscv.h:128
      rd : aliased LibosRiscvRegister;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libriscv.h:128
      imm : aliased lwtypes_h.LwU64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libriscv.h:129
      csr : aliased lwtypes_h.LwU64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libriscv.h:130
   end record
   with Convention => C_Pass_By_Copy;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libriscv.h:131

   function libosRiscvDecode
     (opcode : lwtypes_h.LwU32;
      programCounter : lwtypes_h.LwU64;
      instr : access LibosRiscvInstruction) return lwtypes_h.LwU64  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libriscv.h:133
   with Import => True, 
        Convention => C, 
        External_Name => "libosRiscvDecode";

   procedure libosRiscvPrint (pc : lwtypes_h.LwU64; i : access LibosRiscvInstruction)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libriscv.h:134
   with Import => True, 
        Convention => C, 
        External_Name => "libosRiscvPrint";

end libriscv_h;
