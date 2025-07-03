/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef RISCV_ISA_H
#define RISCV_ISA_H

#include <lwtypes.h>


typedef enum {
  RISCV_X0 = 0,
  RISCV_RA = 1,
  RISCV_SP = 2,
  RISCV_GP = 3,
  RISCV_TP = 4,
  RISCV_T0 = 5,
  RISCV_T1 = 6,
  RISCV_T2 = 7,
  RISCV_S0 = 8,
  RISCV_S1 = 9,
  RISCV_A0 = 10,
  RISCV_A1 = 11,
  RISCV_A2 = 12,
  RISCV_A3 = 13,
  RISCV_A4 = 14,
  RISCV_A5 = 15,
  RISCV_A6 = 16,
  RISCV_A7 = 17,
  RISCV_S2 = 18,
  RISCV_S3 = 19,
  RISCV_S4 = 20,
  RISCV_S5 = 21,
  RISCV_S6 = 22,
  RISCV_S7 = 23,
  RISCV_S8 = 24,
  RISCV_S9 = 25,
  RISCV_S10= 26,
  RISCV_S11= 27,
  RISCV_T3 = 28,
  RISCV_T4 = 29,
  RISCV_T5 = 30,
  RISCV_T6 = 31,
} LibosRiscvRegister;

typedef enum {
  RISCV_LB,
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
  RISCV_REMUW,
  RISCV_FENCE
} LibosRiscvOpcode;

typedef struct {
  LibosRiscvOpcode    opcode;
  LibosRiscvRegister  rs1, rs2, rd;
  LwU64               imm;
  LwU64               csr;
} LibosRiscvInstruction;

#ifdef __cplusplus
extern "C" {
#endif
  LwU64 libosRiscvDecode(LwU32 opcode, LwU64 programCounter, LibosRiscvInstruction * instr);
  void  libosRiscvPrint(LwU64 pc, LibosRiscvInstruction * i);
  extern const char * opcode_names[256];
#ifdef __cplusplus
}
#endif
#endif