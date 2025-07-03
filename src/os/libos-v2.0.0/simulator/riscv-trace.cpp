/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "riscv-trace.h"
#include <drivers/common/inc/hwref/ampere/ga100/dev_gsp_riscv_csr_64.h>
#include <drivers/common/inc/swref/ampere/ga100/dev_riscv_csr_64_addendum.h>

#define TRACE_PRINT(...)      printf(__VA_ARGS__)
#define TRACE_ADDRESS(r)      print_address(r)
#define TRACE_REGISTER(r)     print_register(r)
#define TRACE_OUT_REGISTER(r) print_out_register(r)

// Ansi colors
#define WHITE    "\u001b[37m"
#define BR_GREEN "\u001b[32;1m"
#define BR_BLUE  "\u001b[34;1m"

static const char *register_names[32] = {
    WHITE "x0" WHITE,   BR_GREEN "ra" WHITE, BR_GREEN "sp" WHITE, BR_GREEN "gp" WHITE, BR_GREEN "tp" WHITE,
    BR_BLUE "t0" WHITE, BR_BLUE "t1" WHITE,  BR_BLUE "t2" WHITE,  BR_BLUE "s0" WHITE,  BR_BLUE "s1" WHITE,
    BR_BLUE "a0" WHITE, BR_BLUE "a1" WHITE,  BR_BLUE "a2" WHITE,  BR_BLUE "a3" WHITE,  BR_BLUE "a4" WHITE,
    BR_BLUE "a5" WHITE, BR_BLUE "a6" WHITE,  BR_BLUE "a7" WHITE,  BR_BLUE "s2" WHITE,  BR_BLUE "s3" WHITE,
    BR_BLUE "s4" WHITE, BR_BLUE "s5" WHITE,  BR_BLUE "s6" WHITE,  BR_BLUE "s7" WHITE,  BR_BLUE "s8" WHITE,
    BR_BLUE "s9" WHITE, BR_BLUE "s10" WHITE, BR_BLUE "s11" WHITE, BR_BLUE "t3" WHITE,  BR_BLUE "t4" WHITE,
    BR_BLUE "t5" WHITE, BR_BLUE "t6" WHITE};

#define MOVE_TO_COLUMN_40 "\r\u001b[55C\u001b[35m"

riscv_tracer::riscv_tracer(riscv_core *core) : core(core)
{
    core->event_fast_forward.connect(&riscv_tracer::handler_fast_forward, this);
    core->event_mret.connect(&riscv_tracer::handler_mret, this);
    core->event_instruction_start.connect(&riscv_tracer::handler_instruction_start, this);
    core->event_instruction_retired.connect(&riscv_tracer::handler_instruction_retired, this);
    core->event_ebreak.connect(&riscv_tracer::handler_ebreak, this);
    core->event_wfi.connect(&riscv_tracer::handler_wfi, this);
    core->event_ecall.connect(&riscv_tracer::handler_ecall, this);
    core->event_csr.connect(&riscv_tracer::handler_csr, this);
    core->event_csri.connect(&riscv_tracer::handler_csri, this);
    core->event_lui.connect(&riscv_tracer::handler_lui, this);
    core->event_auipc.connect(&riscv_tracer::handler_auipc, this);
    core->event_jalr.connect(&riscv_tracer::handler_jalr, this);
    core->event_jal.connect(&riscv_tracer::handler_jal, this);
    core->event_branch.connect(&riscv_tracer::handler_branch, this);
    core->event_trap.connect(&riscv_tracer::handler_trap, this);
    core->event_arith_immediate.connect(&riscv_tracer::handler_arith_immediate, this);
    core->event_arithmetic.connect(&riscv_tracer::handler_arithmetic, this);
    core->event_load.connect(&riscv_tracer::handler_load, this);
    core->event_store.connect(&riscv_tracer::handler_store, this);
    core->event_register_written.connect(&riscv_tracer::handler_register_written, this);
}

void riscv_tracer::print_address(LwU64 address)
{
    const char *symbol, *directory, *filename;
    LwU64 out_line = 0, resolve_pc, symbolOffset, out_col, lineOffset;

    if (core->mstatus & DRF_DEF(_RISCV_CSR, _MSTATUS, _VM, _LWMPU))
        resolve_pc = address; // We're operating with MPU enabled
    else
        resolve_pc = 0; // Don't attempt to resolve

    TRACE_PRINT("\u001b[35mtarget:%08llx", address);

    TRACE_PRINT("(");
    if (libosDebugResolveSymbolToName(&core->firmware->resolver, resolve_pc, &symbol, &symbolOffset))
        TRACE_PRINT("%s+0x%llx", symbol, symbolOffset);

    if (libosDwarfResolveLine(
            &core->firmware->resolver, resolve_pc, &directory, &filename, &out_line, &out_col, &lineOffset))
        TRACE_PRINT("@ %s:%lld", filename, out_line);

    TRACE_PRINT(")");
}

void riscv_tracer::print_register(LwU32 r)
{
    if (r != 0)
        TRACE_PRINT("%s:\u001b[31m%016llx ", register_names[r], core->regs[r]);
}

void riscv_tracer::print_out_register(LwU32 r)
{
    if (r != 0)
        TRACE_PRINT("\u001b[32m%s:\u001b[32m%016llx ", register_names[r], core->regs[r]);
}

void riscv_tracer::handler_store(store_kind kind, LwU64 rs1, LwU64 rs2, LwS64 imm)
{
    switch (kind)
    {
    case SB:
        TRACE_PRINT("sb");
        break;
    case SH:
        TRACE_PRINT("sh");
        break;
    case SW:
        TRACE_PRINT("sw");
        break;
    case SD:
        TRACE_PRINT("sd");
        break;
    default:
        assert(0);
    }
    TRACE_PRINT(
        "    %s, \u001b[31m#%04llx\u001b[37m(%s)" MOVE_TO_COLUMN_40, register_names[rs2], imm,
        register_names[rs1]);
    TRACE_REGISTER(rs1);
}

void riscv_tracer::handler_load(load_kind kind, LwU64 rd, LwU64 rs1, LwS64 imm)
{
    switch (kind)
    {
    case LB:
        TRACE_PRINT("lb");
        break;
    case LH:
        TRACE_PRINT("lh");
        break;
    case LW:
        TRACE_PRINT("lw");
        break;
    case LD:
        TRACE_PRINT("ld");
        break;
    case LBU:
        TRACE_PRINT("lbu");
        break;
    case LHU:
        TRACE_PRINT("lhu");
        break;
    case LWU:
        TRACE_PRINT("lwu");
        break;
    default:
        assert(0);
    }
    TRACE_PRINT(
        "    %s, \u001b[31m#%04llx\u001b[37m(%s)" MOVE_TO_COLUMN_40, register_names[rd], imm,
        register_names[rs1]);
    TRACE_REGISTER(rs1);
}

void riscv_tracer::handler_instruction_start()
{
    LwU64 resolve_pc;

    if (core->mpu_translation_enabled)
        resolve_pc = core->pc;
    else
        resolve_pc = core->pc - core->elf_address +
                     ((elf64_header *)(&core->firmware->elf[0]))->entry; // really should use PHDR[0] here

    const char *symbol, *directory, *filename;
    LwU64 out_line = 0, symbolOffset, out_col, lineOffset;
    LwBool symbolMatched =
        libosDebugResolveSymbolToName(&core->firmware->resolver, resolve_pc, &symbol, &symbolOffset);
    LwBool lineMatched = libosDwarfResolveLine(
        &core->firmware->resolver, resolve_pc, &directory, &filename, &out_line, &out_col, &lineOffset);

    if (!printed_symbol_since_last_branch || (symbolMatched && symbolOffset == 0) ||
        (lineMatched && lineOffset == 0))
    {
        TRACE_PRINT("\n\u001b[37m%s: \u001b[32m;", core->privilege == privilege_machine ? "M" : core->privilege == privilege_supervisor ? "S" : "U");

        if (symbolMatched)
            TRACE_PRINT(" %s+0x%llx", symbol, symbolOffset);

        if (lineMatched)
            TRACE_PRINT(" @ %s:%lld", filename, out_line);

        TRACE_PRINT("\n");
    }

    TRACE_PRINT(
        "\u001b[37m%s: \u001b[36m%08llx \u001b[37m", core->privilege == privilege_machine ? "M" : core->privilege == privilege_supervisor ? "S" : "U",
        core->pc);
    printed_symbol_since_last_branch = true;
}

void riscv_tracer::handler_instruction_retired() { TRACE_PRINT("\n"); }

void riscv_tracer::handler_register_written(LwU64 rd) { TRACE_OUT_REGISTER(rd); }

void riscv_tracer::handler_arithmetic(arithmetic_kind kind, LwU64 rd, LwU64 rs1, LwU64 rs2)
{
    switch (kind)
    {
    case ADDW:
        TRACE_PRINT("addw");
        break;
    case SUBW:
        TRACE_PRINT("subw");
        break;
    case MULW:
        TRACE_PRINT("mulw");
        break;
    case SLLW:
        TRACE_PRINT("sllw");
        break;
    case DIVW:
        TRACE_PRINT("divw");
        break;
    case SRLW:
        TRACE_PRINT("srlw");
        break;
    case SRAW:
        TRACE_PRINT("sraw");
        break;
    case DIVUW:
        TRACE_PRINT("divuw");
        break;
    case REMW:
        TRACE_PRINT("remw");
        break;
    case REMUW:
        TRACE_PRINT("remuw");
        break;
    case ADD:
        TRACE_PRINT("add");
        break;
    case SUB:
        TRACE_PRINT("sub");
        break;
    case MUL:
        TRACE_PRINT("mul");
        break;
    case SLL:
        TRACE_PRINT("sll");
        break;
    case SLT:
        TRACE_PRINT("slt");
        break;
    case SLTU:
        TRACE_PRINT("sltu");
        break;
    case DIV:
        TRACE_PRINT("div");
        break;
    case XOR:
        TRACE_PRINT("xor");
        break;
    case SRL:
        TRACE_PRINT("srl");
        break;
    case SRA:
        TRACE_PRINT("sra");
        break;
    case DIVU:
        TRACE_PRINT("divu");
        break;
    case REM:
        TRACE_PRINT("rem");
        break;
    case OR:
        TRACE_PRINT("or");
        break;
    case AND:
        TRACE_PRINT("and");
        break;
    case REMU:
        TRACE_PRINT("remu");
        break;
    default:
        assert(0);
        break;
    }
    TRACE_PRINT(
        "   %s, %s, %s" MOVE_TO_COLUMN_40, register_names[rd], register_names[rs1], register_names[rs2]);
    TRACE_REGISTER(rs1);
    TRACE_REGISTER(rs2);
}

void riscv_tracer::handler_arith_immediate(arithmetic_imm_kind kind, LwU64 rd, LwU64 rs1, LwS64 imm)
{
    switch (kind)
    {
    case ADDI:
        TRACE_PRINT("addi");
        break;

    case SLLI:
        TRACE_PRINT("slli");
        break;

    case SLTI:
        TRACE_PRINT("slti");
        break;

    case SLTIU:
        TRACE_PRINT("sltiu");
        break;

    case XORI:
        TRACE_PRINT("xori");
        break;

    case SRLI:
        TRACE_PRINT("srli");
        break;

    case SRAI:
        TRACE_PRINT("srai");
        break;

    case ORI:
        TRACE_PRINT("ori");
        break;

    case ANDI:
        TRACE_PRINT("andi");
        break;

    case ADDIW:
        TRACE_PRINT("addiw");
        break;

    case SLLIW:
        TRACE_PRINT("slliw");
        break;

    case SRLIW:
        TRACE_PRINT("srliw");
        break;

    case SRAIW:
        TRACE_PRINT("sraiw");
        break;

    default:
        assert(0);
    }
    TRACE_PRINT(
        "   %s, %s, \u001b[31m#%04llx\u001b[37m" MOVE_TO_COLUMN_40, register_names[rd], register_names[rs1],
        imm);
    TRACE_REGISTER(rs1);
}

void riscv_tracer::handler_fast_forward(LwU64 new_cycle)
{
    TRACE_PRINT("\u001b[33m FAST FORWARDING %lld %lld\n", core->mtimecmp, core->mtime());
}

void riscv_tracer::handler_trap(LwU64 new_mcause, LwU64 new_mbadaddr)
{
    if (new_mcause == DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _ILL))
    {
        TRACE_PRINT(" \u001b[33m ILLEGAL TRAP");
    }
    else if (new_mcause == DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _SACC_FAULT))
    {
        TRACE_PRINT(" \u001b[33m STORE ACCESS TRAP\n");
    }
    else if (new_mcause == DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _LACC_FAULT))
    {
        TRACE_PRINT(" \u001b[33m LOAD ACCESS TRAP\n");
    }
    else if (new_mcause == (REF_NUM64(LW_RISCV_CSR_MCAUSE_INT, 1) | LW_RISCV_CSR_MCAUSE_EXCODE_M_TINT))
    {
        TRACE_PRINT("\u001b[33m INTERRUPT: MACHINE TIMER INTERUPT TRAP\n");
    }
}

void riscv_tracer::handler_auipc(LwU64 rd, LwU64 target)
{
    TRACE_PRINT("auipc  %s, \u001b[31m#%04llx\u001b[37m " MOVE_TO_COLUMN_40, register_names[rd], target);
    TRACE_REGISTER(rd);
    TRACE_ADDRESS(target);
}

void riscv_tracer::handler_lui(LwU64 rd, LwU64 imm)
{
    TRACE_PRINT("lui    %s, \u001b[31m#%08llx\u001b[37m  " MOVE_TO_COLUMN_40, register_names[rd], imm);
    TRACE_REGISTER(rd);
}

void riscv_tracer::handler_jalr(LwU64 rd, LwU64 rs1, LwS64 imm)
{
    printed_symbol_since_last_branch = false;
    TRACE_PRINT(
        "jalr   %s, \u001b[31m#%04llx\u001b[37m(%s)  " MOVE_TO_COLUMN_40, register_names[rd], imm,
        register_names[rs1]);
    TRACE_REGISTER(rd);
    TRACE_REGISTER(rs1);
    TRACE_ADDRESS(core->regs[rs1] + imm);
}

void riscv_tracer::handler_jal(LwU64 rd, LwU64 target)
{
    printed_symbol_since_last_branch = false;
    TRACE_PRINT("jal    %s, \u001b[31m#%04llx\u001b[37m  " MOVE_TO_COLUMN_40, register_names[rd], target);
    TRACE_REGISTER(rd);
    TRACE_ADDRESS(target);
}

void riscv_tracer::handler_branch(branch_kind kind, LwU64 rs1, LwU64 rs2, LwU64 target)
{
    printed_symbol_since_last_branch = false;
    switch (kind)
    {
    case BEQ:
        TRACE_PRINT("beq");
        break;

    case BNE:
        TRACE_PRINT("bne");
        break;

    case BLT:
        TRACE_PRINT("blt");
        break;

    case BGE:
        TRACE_PRINT("bge");
        break;

    case BLTU:
        TRACE_PRINT("bltu");
        break;

    case BGEU:
        TRACE_PRINT("bgeu");
        break;

    default:
        assert(0);
    }
    TRACE_PRINT(
        "    %s, %s, \u001b[31m#%04llx\u001b[37m  " MOVE_TO_COLUMN_40, register_names[rs1],
        register_names[rs2], target);
    TRACE_REGISTER(rs1);
    TRACE_REGISTER(rs2);
    TRACE_ADDRESS(target);
}

void riscv_tracer::handler_csr(csr_kind kind, LwU64 csr, LwU64 rd, LwU64 rs1)
{
    switch (kind)
    {
    case CSRRW:
        TRACE_PRINT("csrrw");
        break;

    case CSRRS:
        TRACE_PRINT("csrrs");
        break;

    case CSRRC:
        TRACE_PRINT("csrrc");
        break;

    default:
        assert(0);
    }
    TRACE_PRINT(
        "  \u001b[31m#%04llx\u001b[37m, %s, %s " MOVE_TO_COLUMN_40, csr, register_names[rd],
        register_names[rs1]);
    TRACE_REGISTER(rd);
    TRACE_REGISTER(rs1);
}

void riscv_tracer::handler_csri(csr_kind kind, LwU64 csr, LwU64 rd, LwU64 imm)
{
    switch (kind)
    {
    case CSRRWI:
        TRACE_PRINT("csrrwi");
        break;

    case CSRRSI:
        TRACE_PRINT("csrrsi");
        break;

    case CSRRCI:
        TRACE_PRINT("csrrci");
        break;

    default:
        assert(0);
    }
    TRACE_PRINT(
        " \u001b[31m#%04llx\u001b[37m, %s, \u001b[31m#%02llx\u001b[37m " MOVE_TO_COLUMN_40, csr,
        register_names[rd], imm);
    TRACE_REGISTER(rd);
}

void riscv_tracer::handler_ecall() { TRACE_PRINT("ecall \u001b[33m ENVIRONMENT TRAP\n"); }

void riscv_tracer::handler_wfi() { TRACE_PRINT("wfi"); }

void riscv_tracer::handler_ebreak() { TRACE_PRINT("ebreak \u001b[33m BREAKPOINT TRAP\n"); }

void riscv_tracer::handler_mret()
{
    if (core->has_supervisor_mode)
      TRACE_PRINT("sret   ");
    else
      TRACE_PRINT("mret   ");
    TRACE_ADDRESS(core->xepc);
    TRACE_PRINT("\n");
}
