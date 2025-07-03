#include "libriscv.h"
#include <lwtypes.h>

#ifdef LIBOS
#   include "libos.h"
#   define assert LibosAssert
#   define printf LibosPrintf
#else
#   include <stdio.h>
#   include <assert.h>
#   define LIBOS_CONFIG_RISCV_COMPRESSED 1
#endif

enum opcode_t
{
    OP_UNIMP                  = 0,
    OP_LD                     = 3,
    OP_SD                     = 35,
    OP_FENCE                  = 15,
    OP_AUIPC                  = 23,
    OP_ARITHMETIC_IMMEDIATE   = 19,
    OP_ARITHMETIC_IMMEDIATE32 = 27,
    OP_ARITHMETIC             = 51,
    OP_ARITHMETIC32           = 59,
    OP_BRANCH                 = 99,
    OP_JAL                    = 111,
    OP_JALR                   = 103,
    OP_LUI                    = 55,
    OP_ELWIRONMENT            = 115
};

static LwU64 sext32(LwS32 t) { return (LwU64)((LwS64)t); }
static LwS32 sextn(LwS32 t, LwU32 width) { return (t << (32 - width)) >> (32 - width); }

LwU64 libosRiscvDecode(LwU32 opcode, LwU64 programCounter, LibosRiscvInstruction * instr)
{
  LwS64 imm;
  LwU64 rs1, rs2, rd, top6, top7, shamt, target, csr, scratch, funct3;
  LwU64 pc  = programCounter + 4;
  rd  = (opcode >> 7) & 0x1f;
  rs1 = (opcode >> 15) & 0x1f;
  rs2 = (opcode >> 20) & 0x1f;
  switch (opcode & 0x7F)
  {
  #if defined(LIBOS_CONFIG_RISCV_COMPRESSED)
  //! Compressed Quadrant 0
  case 0x0000:  case 0x0004:  case 0x0008:  case 0x000C:  case 0x0010:  case 0x0014:  case 0x0018:  case 0x001C:
  case 0x0020:  case 0x0024:  case 0x0028:  case 0x002C:  case 0x0030:  case 0x0034:  case 0x0038:  case 0x003C:
  case 0x0040:  case 0x0044:  case 0x0048:  case 0x004C:  case 0x0050:  case 0x0054:  case 0x0058:  case 0x005C:
  case 0x0060:  case 0x0064:  case 0x0068:  case 0x006C:  case 0x0070:  case 0x0074:  case 0x0078:  case 0x007C:
      funct3 = (opcode >> 13) & 7;
      rd     = ((opcode >> 2) & 7) | 8;
      switch (funct3)
      {
      case 0: // C.ADDI4SPN
          imm = ((opcode >> 0x00000007) & 0x00000030) | ((opcode >> 0x00000001) & 0x000003c0) |
                ((opcode >> 0x00000004) & 0x00000004) | ((opcode >> 0x00000002) & 0x00000008);
          if (imm == 0)
              return 0;
          instr->opcode = RISCV_ADDI, instr->rd = rd, instr->rs1 = RISCV_SP, instr->imm = imm;
          break;

      case 2: // C.LW
          imm = ((opcode >> 0x00000007) & 0x00000038) | ((opcode >> 0x00000004) & 0x00000004) |
                ((opcode << 0x00000001) & 0x00000040);
          rs1 = ((opcode >> 7) & 7) | 8;
          instr->opcode = RISCV_LW, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
          break;

      case 3: // C.LD
          imm = ((opcode >> 0x00000007) & 0x00000038) | ((opcode << 0x00000001) & 0x000000c0);
          rs1 = ((opcode >> 7) & 7) | 8;
          instr->opcode = RISCV_LD, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
          break;

      case 6: // C.SW
          imm = ((opcode >> 0x00000007) & 0x00000038) | ((opcode >> 0x00000004) & 0x00000004) |
                ((opcode << 0x00000001) & 0x00000040);
          rs1 = ((opcode >> 7) & 7) | 8;
          instr->opcode = RISCV_SW, instr->rs2 = rd, instr->rs1 = rs1, instr->imm = imm;
          break;

      case 7: // C.SD
          imm = ((opcode >> 0x00000007) & 0x00000038) | ((opcode << 0x00000001) & 0x000000c0);
          rs1 = ((opcode >> 7) & 7) | 8;
          instr->opcode = RISCV_SD, instr->rs2 = rd, instr->rs1 = rs1, instr->imm = imm;
          break;

      default:
          return 0;
      }
      return programCounter + 2;

  //! Compressed Quadrant 1
  case 0x0001:  case 0x0005:  case 0x0009:  case 0x000D:  case 0x0011:  case 0x0015:  case 0x0019:  case 0x001D:
  case 0x0021:  case 0x0025:  case 0x0029:  case 0x002D:  case 0x0031:  case 0x0035:  case 0x0039:  case 0x003D:
  case 0x0041:  case 0x0045:  case 0x0049:  case 0x004D:  case 0x0051:  case 0x0055:  case 0x0059:  case 0x005D:
  case 0x0061:  case 0x0065:  case 0x0069:  case 0x006D:  case 0x0071:  case 0x0075:  case 0x0079:  case 0x007D:
      switch (funct3 = (opcode >> 13) & 7)
      {
      case 0: // C.ADDI
          imm = sextn(((opcode >> 0x00000007) & 0x00000020) | ((opcode >> 0x00000002) & 0x0000001f), 6);
          instr->opcode = RISCV_ADDI, instr->rd = rd, instr->rs1 = rd, instr->imm = imm;
          break;

      case 1: // C.ADDIW
          imm = sextn(((opcode >> 0x00000007) & 0x00000020) | ((opcode >> 0x00000002) & 0x0000001f), 6);
          instr->opcode = RISCV_ADDIW, instr->rd = rd, instr->rs1 = rd, instr->imm = imm;
          break;

      case 2: // C.LI
          imm = sextn(((opcode >> 0x00000007) & 0x00000020) | ((opcode >> 0x00000002) & 0x0000001f), 6);
          instr->opcode = RISCV_LI, instr->rd = rd, instr->imm = imm;
          break;

      case 3:
          if (rd == RISCV_SP) // C.ADDI16SP
          {
              imm = sextn(
                  ((opcode >> 0x00000003) & 0x00000200) | ((opcode >> 0x00000002) & 0x00000010) |
                      ((opcode << 0x00000001) & 0x00000040) | ((opcode << 0x00000004) & 0x00000180) |
                      ((opcode << 0x00000003) & 0x00000020),
                  10);

              if (imm == 0)
                  return 0;

              instr->opcode = RISCV_ADDI, instr->rd = RISCV_SP, instr->rs1 = RISCV_SP, instr->imm = imm;
          }
          else  // C.LUI
          {
              imm =
                  sextn(((opcode << 0x00000005) & 0x00020000) | ((opcode << 0x0000000a) & 0x0001f000), 18);

              instr->opcode = RISCV_LUI, instr->rd = rd, instr->imm = imm;
          }
          break;

      case 4:
          rd     = ((opcode >> 7) & 7) | 8;
          switch (funct3 = (opcode >> 10) & 3)
          {
          case 0: // C.SRLI
              imm = ((opcode >> 0x00000007) & 0x00000020) | ((opcode >> 0x00000002) & 0x0000001f);
              instr->opcode = RISCV_SRLI, instr->rd = rd, instr->rs1 = rd, instr->imm = imm;
              break;

          case 1: // C.SRAI
              imm = ((opcode >> 0x00000007) & 0x00000020) | ((opcode >> 0x00000002) & 0x0000001f);
              instr->opcode = RISCV_SRAI, instr->rd = rd, instr->rs1 = rd, instr->imm = imm;
              break;

          case 2: // C.ANDI
              imm = sextn(((opcode >> 0x00000007) & 0x00000020) | ((opcode >> 0x00000002) & 0x0000001f), 6);
              instr->opcode = RISCV_ANDI, instr->rd = rd, instr->rs1 = rd, instr->imm = imm;
              break;

          case 3:
              rs2    = ((opcode >> 2) & 7) | 8;
              switch (funct3 = ((opcode >> 5) & 3) | ((opcode >> (12 - 2)) & 4))
              {
              case 0: // C.SUB
                  instr->opcode = RISCV_SUB, instr->rd = rd, instr->rs1 = rd, instr->rs2 = rs2;
                  break;

              case 1: // C.XOR
                  instr->opcode = RISCV_XOR, instr->rd = rd, instr->rs1 = rd, instr->rs2 = rs2;
                  break;

              case 2: // C.OR
                  instr->opcode = RISCV_OR, instr->rd = rd, instr->rs1 = rd, instr->rs2 = rs2;
                  break;

              case 3: // C.AND
                  instr->opcode = RISCV_AND, instr->rd = rd, instr->rs1 = rd, instr->rs2 = rs2;
                  break;

              case 4: // C.SUBW
                  instr->opcode = RISCV_SUBW, instr->rd = rd, instr->rs1 = rd, instr->rs2 = rs2;
                  break;

              case 5: // C.ADDW
                  instr->opcode = RISCV_ADDW, instr->rd = rd, instr->rs1 = rd, instr->rs2 = rs2;
                  break;

              default:
                  return 0;
              }
              break;

              default:
                  return 0;
          }
          break;

      case 5: // C.J
          imm = sextn(
              ((opcode >> 0x00000001) & 0x00000800) | ((opcode >> 0x00000007) & 0x00000010) |
                  ((opcode >> 0x00000001) & 0x00000300) | ((opcode << 0x00000002) & 0x00000400) |
                  ((opcode >> 0x00000001) & 0x00000040) | ((opcode << 0x00000001) & 0x00000080) |
                  ((opcode >> 0x00000002) & 0x0000000e) | ((opcode << 0x00000003) & 0x00000020),
              12);
          instr->opcode = RISCV_JAL, instr->rd = RISCV_X0, instr->imm = programCounter + imm;
          break;

      case 6: // C.BEQZ
          rs1 = ((opcode >> 7) & 7) | 8;
          imm = sextn(
              ((opcode >> 0x00000004) & 0x00000100) | ((opcode >> 0x00000007) & 0x00000018) |
                  ((opcode << 0x00000001) & 0x000000c0) | ((opcode >> 0x00000002) & 0x00000006) |
                  ((opcode << 0x00000003) & 0x00000020),
              9);
          instr->opcode = RISCV_BEQ, instr->rs1 = rs1, instr->rs2 = RISCV_X0, instr->imm = programCounter + imm;
          break;

      case 7: // C.BNEZ
          rs1 = ((opcode >> 7) & 7) | 8;
          imm = sextn(
              ((opcode >> 0x00000004) & 0x00000100) | ((opcode >> 0x00000007) & 0x00000018) |
                  ((opcode << 0x00000001) & 0x000000c0) | ((opcode >> 0x00000002) & 0x00000006) |
                  ((opcode << 0x00000003) & 0x00000020),
              9);
          instr->opcode = RISCV_BNE, instr->rs1 = rs1, instr->rs2 = RISCV_X0, instr->imm = programCounter + imm;
          break;

      default:
          return 0;
      }
      return programCounter + 2;

  //! Compressed Quadrant 2
  case 0x0002:  case 0x0006:  case 0x000A:  case 0x000E:  case 0x0012:  case 0x0016:  case 0x001A:  case 0x001E:
  case 0x0022:  case 0x0026:  case 0x002A:  case 0x002E:  case 0x0032:  case 0x0036:  case 0x003A:  case 0x003E:
  case 0x0042:  case 0x0046:  case 0x004A:  case 0x004E:  case 0x0052:  case 0x0056:  case 0x005A:  case 0x005E:
  case 0x0062:  case 0x0066:  case 0x006A:  case 0x006E:  case 0x0072:  case 0x0076:  case 0x007A:  case 0x007E:
      rs2    = (opcode >> 2) & 0x1f;
      switch (funct3 = (opcode >> 13) & 7)
      {
      case 0: // C.SLLI
          imm = ((opcode >> 0x00000007) & 0x00000020) | rs2;
          instr->opcode = RISCV_SLLI, instr->rd = rd, instr->rs1 = rd, instr->imm = imm;
          break;

      case 2: // C.LWSP
          imm = ((opcode >> 0x00000007) & 0x00000020) | (rs2 & (7 << 2)) |
                ((opcode << 0x00000004) & 0x000000c0);
          instr->opcode = RISCV_LW, instr->rd = rd, instr->rs1 = RISCV_SP, instr->imm = imm;
          break;

      case 3: // C.LDSP
          imm = ((opcode >> 0x00000007) & 0x00000020) | (rs2 & (3 << 3)) |
                ((opcode << 0x00000004) & 0x000001c0);
          instr->opcode = RISCV_LD, instr->rd = rd, instr->rs1 = RISCV_SP, instr->imm = imm;
          break;

      case 4:
          if (((opcode >> 12) & 1) == 0)
          {
              if (rs2 == 0) // C.JR
              {
                  if (rd == 0)
                      return 0;
                  instr->opcode = RISCV_JALR, instr->rd = RISCV_X0, instr->rs1 = rd, instr->imm = 0;
              }
              else        // C.MV
              {
                  instr->opcode = RISCV_ADDI, instr->rd = rd, instr->rs1 = rs2, instr->imm = 0;
              }
          }
          else
          {
              if (rs2 == 0)
              {
                  if (rd == 0)  // C.EBREAK
                  {
                      instr->opcode = RISCV_EBREAK;
                  }
                  else          // C.JALR
                  {
                      instr->opcode = RISCV_JALR, instr->rd = RISCV_RA, instr->rs1 = rd, instr->imm = 0;
                  }
              }
              else  // C.ADD
              {
                  instr->opcode = RISCV_ADD, instr->rd = rd, instr->rs1 = rd, instr->rs2 = rs2;
              }
          }
          break;

      case 6: // C.SWSP
          imm = ((opcode >> 0x00000007) & 0x0000003c) | ((opcode >> 0x00000001) & 0x000000c0);
          instr->opcode = RISCV_SW, instr->rs1 = RISCV_SP, instr->rs2 = rs2, instr->imm = imm;
          break;

      case 7: // C.SDSP
          imm = ((opcode >> 0x00000007) & 0x00000038) | ((opcode >> 0x00000001) & 0x000001c0);
          instr->opcode = RISCV_SD, instr->rs1 = RISCV_SP, instr->rs2 = rs2, instr->imm = imm;
          break;

      default:
          return 0;
      }
      return programCounter+2;
  #endif // LIBOS_CONFIG_RISCV_COMPRESSED

  case OP_ELWIRONMENT:
      csr = opcode >> 20, rs1 = (opcode >> 15) & 31, rd = (opcode >> 7) & 31;
      imm = rs1;
      switch ((opcode >> 12) & 7)
      {
      case 0: // ECALL/EBREAK/MRET/...
          switch (opcode)
          {
          case 0x30200073: // MRET
              instr->opcode = RISCV_MRET;
              break;

          case 0x10200073: // SRET
              instr->opcode = RISCV_SRET;
              break;

          case 0x00100073:
              instr->opcode = RISCV_EBREAK;
              break;

          case 0x10500073:
              instr->opcode = RISCV_WFI;
              break;

          case 0x00000073:
              instr->opcode = RISCV_ECALL;
              break;

          default:
              return 0;
          }
          break;

      case 1: // CSRRW
          instr->opcode = RISCV_CSRRW, instr->csr = csr, instr->rd = rd, instr->rs1 = rs1;
          break;

      case 2: // CSRRS
          instr->opcode = RISCV_CSRRS, instr->csr = csr, instr->rd = rd, instr->rs1 = rs1;
          break;

      case 3: // CSRRC
          instr->opcode = RISCV_CSRRC, instr->csr = csr, instr->rd = rd, instr->rs1 = rs1;
          break;

      case 5: // CSRRWI
          instr->opcode = RISCV_CSRRWI, instr->csr = csr, instr->rd = rd, instr->imm = imm;
          break;

      case 6: // CSRRSI
          instr->opcode = RISCV_CSRRSI, instr->csr = csr, instr->rd = rd, instr->imm = imm;
          break;

      case 7: // CSRRCI
          instr->opcode = RISCV_CSRRCI, instr->csr = csr, instr->rd = rd, instr->imm = imm;
          break;

      default:
          return 0;
      }
      return programCounter+4;

  case OP_LUI:
      rd = (opcode >> 7) & 31, imm = (((LwS32)opcode) >> 12) << 12;
      instr->opcode = RISCV_LUI, instr->rd = rd, instr->imm = imm;
      return programCounter + 4;

  case OP_JALR:
      //assert(((opcode >> 12) & 7) == 0);
      rd = (opcode >> 7) & 31, rs1 = (opcode >> 15) & 31, imm = ((LwS32)opcode) >> 20;
      instr->opcode = RISCV_JALR, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
      return programCounter + 4;

  case OP_AUIPC:
      rd     = (opcode >> 7) & 31;
      target = programCounter + ((((LwS32)opcode) >> 12) << 12);
      instr->opcode = RISCV_AUIPC, instr->rd = rd, instr->imm = target;
      return programCounter + 4;

  case OP_JAL:
      rd = (opcode >> 7) & 31;
      // imm[20|10:1|11|19:12
      imm = ((LwS32)(
                (opcode & 0x80000000) | ((opcode & 0xFF000) << 11) | ((opcode & 0x00100000) << 2) |
                ((opcode & 0x7FE00000) >> 9))) >>
            12;
      target = programCounter + imm * 2;
      instr->opcode = RISCV_JAL, instr->rd = rd, instr->imm = target;
      return programCounter + 4;

  case OP_BRANCH:
      // Genius: The immediate is swizzled to high heaven

      imm = ((LwS32)(
                ((opcode & 0x7E000000) >> 1) | (opcode & 0x80000000) | ((opcode & 0x80) << (30 - 7)) |
                ((opcode & 0xF00) << 12))) >>
            19;

      rs1 = (opcode >> 15) & 31, rs2 = (opcode >> 20) & 31;
      target = programCounter + imm;
      switch ((opcode >> 12) & 7)
      {
      case 0: // BEQ
          instr->opcode = RISCV_BEQ, instr->rs1 = rs1, instr->rs2 = rs2, instr->imm = target;
          return programCounter + 4;

      case 1: // BNE
          instr->opcode = RISCV_BNE, instr->rs1 = rs1, instr->rs2 = rs2, instr->imm = target;
          return programCounter + 4;

      case 4: // BLT
          instr->opcode = RISCV_BLT, instr->rs1 = rs1, instr->rs2 = rs2, instr->imm = target;
          return programCounter + 4;

      case 5: // BGE
          instr->opcode = RISCV_BGE, instr->rs1 = rs1, instr->rs2 = rs2, instr->imm = target;
          return programCounter + 4;

      case 6: // BLTU
          instr->opcode = RISCV_BLTU, instr->rs1 = rs1, instr->rs2 = rs2, instr->imm = target;
          return programCounter + 4;

      case 7: // BGEU
          instr->opcode = RISCV_BGEU, instr->rs1 = rs1, instr->rs2 = rs2, instr->imm = target;
          return programCounter + 4;

      default:
          return 0;
      }
      break;

  case OP_LD:
      imm = ((LwS32)opcode) >> 20, rs1 = (opcode >> 15) & 31, rd = (opcode >> 7) & 31;
      switch ((opcode >> 12) & 7)
      {
      case 0: // LB
          instr->opcode = RISCV_LB, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
          return programCounter + 4;

      case 1: // LH
          instr->opcode = RISCV_LH, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
          return programCounter + 4;

      case 2: // LW
          instr->opcode = RISCV_LW, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
          return programCounter + 4;

      case 3: // LD
          instr->opcode = RISCV_LD, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
          return programCounter + 4;

      case 4: // LBU
          instr->opcode = RISCV_LBU, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
          return programCounter + 4;

      case 5: // LHU
          instr->opcode = RISCV_LHU, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
          return programCounter + 4;

      case 6: // LWU
          instr->opcode = RISCV_LWU, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
          return programCounter + 4;

      default:
          return 0;
      }
      break;

  case OP_SD:
      imm = ((LwS32)(((opcode & 0xF80) << 13) | (opcode & 0xFE000000))) >> 20;
      rs1 = (opcode >> 15) & 31, rs2 = (opcode >> 20) & 31;

      switch ((opcode >> 12) & 7)
      {
      case 0: // SB
          instr->opcode = RISCV_SB, instr->rs1 = rs1, instr->rs2 = rs2, instr->imm = imm;
          return programCounter + 4;

      case 1: // SH
          instr->opcode = RISCV_SH, instr->rs1 = rs1, instr->rs2 = rs2, instr->imm = imm;
          return programCounter + 4;

      case 2: // SW
          instr->opcode = RISCV_SW, instr->rs1 = rs1, instr->rs2 = rs2, instr->imm = imm;
          return programCounter + 4;

      case 3: // SD
          instr->opcode = RISCV_SD, instr->rs1 = rs1, instr->rs2 = rs2, instr->imm = imm;
          return programCounter + 4;

      default:
          return 0;
      }
      break;

  case OP_ARITHMETIC_IMMEDIATE:
      imm = ((LwS32)opcode) >> 20, rs1 = (opcode >> 15) & 31, rd = (opcode >> 7) & 31;
      shamt = (opcode >> 20) & 63;
      top7  = opcode >> (32 - 7);
      top6  = opcode >> (32 - 6);
      switch ((opcode >> 12) & 7)
      {
      case 0: // ADDI
          instr->opcode = RISCV_ADDI, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
          return programCounter + 4;

      case 1: // SLLI
          if (top6 != 0)
            return 0;
          instr->opcode = RISCV_SLLI, instr->rd = rd, instr->rs1 = rs1, instr->imm = shamt;
          return programCounter + 4;

      case 2: // SLTI
          instr->opcode = RISCV_SLTI, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
          return programCounter + 4;

      case 3: // SLTIU
          instr->opcode = RISCV_SLTIU, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
          return programCounter + 4;

      case 4: // XORI
          instr->opcode = RISCV_XORI, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
          return programCounter + 4;

      case 5:
          if (top6 == 0) // SRLI
          {
              instr->opcode = RISCV_SRLI, instr->rd = rd, instr->rs1 = rs1, instr->imm = shamt;
              return programCounter + 4;
          }
          else if (top6 == 0x10) // SRAI
          {
              instr->opcode = RISCV_SRAI, instr->rd = rd, instr->rs1 = rs1, instr->imm = shamt;
              return programCounter + 4;
          }
          else
              return 0;
          break;

      case 6: // ORI
          instr->opcode = RISCV_ORI, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
          return programCounter + 4;

      case 7: // ANDI
          instr->opcode = RISCV_ANDI, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
          return programCounter + 4;
      }
      break;

  case OP_ARITHMETIC_IMMEDIATE32:
      imm = ((LwS32)opcode) >> 20, rs1 = (opcode >> 15) & 31, rd = (opcode >> 7) & 31;
      shamt = (opcode >> 20) & 63;
      top7  = opcode >> (32 - 7);
      top6  = opcode >> (32 - 6);
      switch ((opcode >> 12) & 7)
      {
      case 0: // ADDIW
          instr->opcode = RISCV_ADDIW, instr->rd = rd, instr->rs1 = rs1, instr->imm = imm;
          return programCounter + 4;

      case 1: // SLLIW
          if (top7 != 0)
            return 0;
          instr->opcode = RISCV_SLLIW, instr->rd = rd, instr->rs1 = rs1, instr->imm = shamt;
          return programCounter + 4;

      case 5:
          if (top7 == 0) // SRLIW
          {
              instr->opcode = RISCV_SRLIW, instr->rd = rd, instr->rs1 = rs1, instr->imm = shamt;
              return programCounter + 4;
          }
          else if (top7 == 0x20) // SRAIW
          {
              instr->opcode = RISCV_SRAIW, instr->rd = rd, instr->rs1 = rs1, instr->imm = shamt;
              return programCounter + 4;
          }
          else
              return 0;
          break;

      default:
          return 0;
      }
      break;

  /*
        RV64I extension adds dedicated 32-bit operand instructions
  */
  case OP_ARITHMETIC32:
      top7 = opcode >> (32 - 7);
      rs2 = (opcode >> 20) & 31, rs1 = (opcode >> 15) & 31, rd = (opcode >> 7) & 31;
      shamt = (opcode >> 20) & 63;
      switch ((opcode >> 12) & 7)
      {
      case 0:
          if (top7 == 0) // ADDW
          {
              instr->opcode = RISCV_ADDW, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else if (top7 == 0x20) // SUBW
          {
              instr->opcode = RISCV_SUBW, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else if (top7 == 0x01) // MULW
          {
              instr->opcode = RISCV_MULW, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else
          {
              return 0;
          }
          break;

      case 1: // SLLW
          if (top7 != 0)
            return 0;

          instr->opcode = RISCV_SLLW, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
          return programCounter + 4;

      case 4:
          if (top7 == 0x01) // DIVW
          {
              instr->opcode = RISCV_DIVW, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else
              return 0;
          break;

      case 5:
          if (top7 == 0) // SRLW
          {
              instr->opcode = RISCV_SRLW, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else if (top7 == 0x20) // SRAW
          {
              instr->opcode = RISCV_SRAW, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else if (top7 == 0x01) // DIVUW
          {
              instr->opcode = RISCV_DIVUW, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else
              return 0;
          break;

      case 6:
          if (top7 == 0x01) // REMW
          {
              instr->opcode = RISCV_REMW, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else
              return 0;
          break;

      case 7:
          if (top7 == 0x01) // REMUW
          {
              instr->opcode = RISCV_REMUW, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else
              return 0;
          break;

      default:
          return 0;
      }
      break;

  case OP_FENCE:
    instr->opcode = RISCV_FENCE;
    return programCounter+4;
    break;

  case OP_ARITHMETIC:
      top7 = opcode >> (32 - 7);
      rs2 = (opcode >> 20) & 31, rs1 = (opcode >> 15) & 31, rd = (opcode >> 7) & 31;
      shamt = (opcode >> 20) & 63;
      switch ((opcode >> 12) & 7)
      {
      case 0:
          if (top7 == 0) // ADD
          {
              instr->opcode = RISCV_ADD, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else if (top7 == 0x20) // SUB
          {
              instr->opcode = RISCV_SUB, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else if (top7 == 0x01) // MUL
          {
              instr->opcode = RISCV_MUL, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else
          {
              return 0;
          }
          break;

      case 1: // SLL
          if (top7 != 0)
            return 0;
          instr->opcode = RISCV_SLL, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
          return programCounter + 4;

      case 2: // SLT
          if(top7 != 0)
            return 0;
          instr->opcode = RISCV_SLT, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
          return programCounter + 4;

      case 3: // SLTU
          if (top7 != 0)
            return 0;
          instr->opcode = RISCV_SLTU, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
          return programCounter + 4;

      case 4:
          if (top7 == 0x01) // DIV
          {
              instr->opcode = RISCV_DIV, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else if (top7 == 0) // XOR
          {
            instr->opcode = RISCV_XOR, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
            return programCounter + 4;
          }
          else
              return 0;
          break;

      case 5:
          if (top7 == 0) // SRL
          {
              instr->opcode = RISCV_SRL, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else if (top7 == 0x20) // SRA
          {
              instr->opcode = RISCV_SRA, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else if (top7 == 0x01) // DIVU
          {
              instr->opcode = RISCV_DIVU, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else
              return 0;
          break;

      case 6:
          if (top7 == 0x01) // REM
          {
              instr->opcode = RISCV_REM, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else if (top7 == 0)
          { // OR
              instr->opcode = RISCV_OR, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else
              return 0;
          break;

      case 7:
          if (top7 == 0) // AND
          {
              instr->opcode = RISCV_AND, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          else if (top7 == 1) // REMU
          {
              instr->opcode = RISCV_REMU, instr->rd = rd, instr->rs1 = rs1, instr->rs2 = rs2;
              return programCounter + 4;
          }
          break;
      }
      break;

  default:
      return 0;
  }
}

static const char *register_names[32] = {
   "x0", "ra",  "sp",   "gp",  "tp",
   "t0", "t1",  "t2",   "s0",  "s1",
   "a0", "a1",  "a2",   "a3",  "a4",
   "a5", "a6",  "a7",   "s2",  "s3",
   "s4", "s5",  "s6",   "s7",  "s8",
   "s9", "s10", "s11",  "t3",  "t4",
   "t5", "t6"
};

const char * opcode_names[256] = {
  [RISCV_LB] = "lb", [RISCV_LH] = "lh", [RISCV_LW] = "lw", 
  [RISCV_LD] = "ld", [RISCV_LBU] = "lbu",  [RISCV_LHU] = "lhu", 
  [RISCV_LWU] = "lwu", [RISCV_SB] = "sb", [RISCV_SH] = "sh", 
  [RISCV_SW] = "sw", [RISCV_SD] = "sd",  [RISCV_LI] = "li",
  [RISCV_LUI] = "lui", [RISCV_AUIPC] = "auipc",
  [RISCV_SRLI] = "srli", [RISCV_SRLIW] = "srliw", 
  [RISCV_SRAI] = "srai", [RISCV_SRAIW] = "sraiw", 
  [RISCV_SLLW] = "sllw", [RISCV_DIV] = "div",
  [RISCV_DIVW] = "divw", [RISCV_DIVU] = "divu", 
  [RISCV_DIVUW] = "divuw", [RISCV_REM] = "rem", 
  [RISCV_REMU] = "remu", [RISCV_SRLW] = "srlw",
  [RISCV_SRAW] = "sraw", [RISCV_SLL] = "sll", 
  [RISCV_SRL] = "srl", [RISCV_SRA] = "sra", 
  [RISCV_SLLI] = "slli", [RISCV_SLLIW] = "slliw",
  [RISCV_SLTI] = "slti", [RISCV_SLTIU] = "sltiu", 
  [RISCV_XORI] = "xori", [RISCV_ORI] = "ori", 
  [RISCV_SUB] = "sub", [RISCV_SUBW] = "subw",
  [RISCV_ADDW] = "addw", [RISCV_MULW] = "mulw", 
  [RISCV_MUL] = "mul", [RISCV_XOR] = "xor", 
  [RISCV_OR] = "or", [RISCV_AND] = "and",
  [RISCV_ADD] = "add", [RISCV_ADDW] = "addw", 
  [RISCV_ADDI] = "addi", [RISCV_ADDIW] = "addiw", 
  [RISCV_ANDI] = "andi", 
  [RISCV_JAL] = "jal", [RISCV_JALR] = "jalr", 
  [RISCV_BEQ] = "beq", [RISCV_BNE] = "bne", 
  [RISCV_BLT] = "blt", [RISCV_BGE] = "bge", 
  [RISCV_BLTU] = "bltu", [RISCV_BGEU] = "bgeu", 
  [RISCV_EBREAK] = "ebreak", [RISCV_MRET] = "mret", 
  [RISCV_SRET] = "sret", [RISCV_WFI] = "wfi", 
  [RISCV_ECALL] = "ecall", [RISCV_CSRRW] = "csrrw", 
  [RISCV_CSRRS] = "csrrs", [RISCV_CSRRC] = "csrrc",  
  [RISCV_CSRRWI] = "csrrwi", [RISCV_CSRRSI] = "csrrsi", 
  [RISCV_CSRRCI] = "csrrci", [RISCV_REMW] = "remw", 
  [RISCV_REMUW] = "remuw"
};

void libosRiscvPrint(LwU64 pc, LibosRiscvInstruction * i)
{
  switch (i->opcode)
  {
    case RISCV_LB: case RISCV_LH: case RISCV_LW: case RISCV_LD: case RISCV_LBU: case RISCV_LHU: case RISCV_LWU:
      printf("%s %s, %lld(%s)\n", opcode_names[i->opcode], register_names[i->rd], i->imm, register_names[i->rs1]);
      break;

    case RISCV_SB: case RISCV_SH: case RISCV_SW: case RISCV_SD:
      printf("%s %s, %lld(%s)\n", opcode_names[i->opcode], register_names[i->rs2], i->imm, register_names[i->rs1]);
      break;

    case RISCV_LI: 
      printf("%s %s, %lld\n", opcode_names[i->opcode], register_names[i->rd], i->imm);
      break;

    case RISCV_LUI: 
      printf("%s %s, %d\n", opcode_names[i->opcode], register_names[i->rd], (LwU32)i->imm / 4096);
      break;

    case RISCV_AUIPC:
      printf("%s %s, %d\n", opcode_names[i->opcode], register_names[i->rd], (LwU32)(i->imm-pc) / 4096);
      break;

    case RISCV_SRLI: case RISCV_SRLIW: case RISCV_SRAI: case RISCV_SRAIW:
    case RISCV_SLLI: case RISCV_SLLIW: case RISCV_SLTI: case RISCV_SLTIU: case RISCV_XORI:
    case RISCV_ORI:  case RISCV_ADDI: case RISCV_ADDIW: case RISCV_ANDI: 
      printf("%s %s, %s, %lld\n", opcode_names[i->opcode], register_names[i->rd], register_names[i->rs1], i->imm);
      break;

    case RISCV_SLLW: case RISCV_DIV: case RISCV_DIVW: case RISCV_DIVU: case RISCV_DIVUW:
    case RISCV_REM: case RISCV_REMU: case RISCV_SRLW: case RISCV_SRAW: case RISCV_SLL:
    case RISCV_SRL: case RISCV_SRA:  case RISCV_SUB:  case RISCV_SUBW: case RISCV_ADDW:
    case RISCV_MULW: case RISCV_MUL: case RISCV_XOR:  case RISCV_OR:   case RISCV_AND:
    case RISCV_ADD: case RISCV_REMW: case RISCV_REMUW:
      printf("%s %s, %s, %s\n", opcode_names[i->opcode], register_names[i->rd], register_names[i->rs1], register_names[i->rs2]);
      break;
     
    case RISCV_EBREAK: case RISCV_MRET: case RISCV_SRET: case RISCV_WFI:
    case RISCV_ECALL:
      printf("%s\n", opcode_names[i->opcode]);
      break;


    /*RISCV_JAL,
    RISCV_JALR,
    RISCV_BEQ,
    RISCV_BNE,
    RISCV_BLT,
    RISCV_BGE,
    RISCV_BLTU,
    RISCV_BGEU,
    RISCV_CSRRW,
    RISCV_CSRRS,
    RISCV_CSRRWI,
    RISCV_CSRRSI,
    RISCV_CSRRCI,*/
  }
}

