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
#include "libos-config.h"
#include "peregrine-headers.h"
#ifdef __MINGW64__
#include <windows.h>
#endif

#define TRACE_PRINT(...)      printf(__VA_ARGS__)
#define TRACE_ADDRESS(r)      print_address(r, false)
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

#define MOVE_TO_COLUMN_40 "\r\u001b[75C\u001b[35m"

riscv_tracer::riscv_tracer(riscv_core *core) : core(core)
{
#ifdef __MINGW64__
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hStdOut, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hStdOut, mode);
#endif

    core->event_fast_forward.connect(&riscv_tracer::handler_fast_forward, this);
    core->event_instruction_start.connect(&riscv_tracer::handler_instruction_start, this);
    core->event_instruction_retired.connect(&riscv_tracer::handler_instruction_retired, this);
    core->event_trap.connect(&riscv_tracer::handler_trap, this);
    core->event_instruction_decoded.connect(&riscv_tracer::instruction_decoded, this);
    core->event_register_written.connect(&riscv_tracer::handler_register_written, this);
}

void riscv_tracer::instruction_decoded(LwU64 pc, LibosRiscvInstruction * i)
{
    switch(i->opcode)
    {
        case RISCV_LB: case RISCV_LH: case RISCV_LW: case RISCV_LD: case RISCV_LBU: case RISCV_LHU: case RISCV_LWU:
        {
            TRACE_PRINT(opcode_names[i->opcode]);

            LwU64 va = core->regs[i->rs1]+i->imm;
            LwU64 pa = 0;
            riscv_core::TranslationKind mask;
            core->translate_address(va, pa, mask);

            TRACE_PRINT(
                "    %s, \u001b[31m#%04llx\u001b[37m(%s) " MOVE_TO_COLUMN_40 " va:%08llx pa:%08llx ", register_names[i->rd], i->imm,
                register_names[i->rs1], va, pa);
            TRACE_REGISTER(i->rs1);            
            break;
        }

        case RISCV_SB: case RISCV_SH: case RISCV_SW: case RISCV_SD:
        {
            TRACE_PRINT(opcode_names[i->opcode]);

            LwU64 va = core->regs[i->rs1]+i->imm;
            LwU64 pa = 0;
            riscv_core::TranslationKind mask;
            core->translate_address(va, pa, mask);

            TRACE_PRINT(
                "    %s, \u001b[31m#%04llx\u001b[37m(%s)" MOVE_TO_COLUMN_40 " va:%08llx pa:%08llx ", register_names[i->rs2], i->imm,
                register_names[i->rs1],  va, pa);

            TRACE_REGISTER(i->rs1);
            break;
        }

        case RISCV_LI: 
        case RISCV_LUI: 
            TRACE_PRINT("lui    %s, \u001b[31m#%08llx\u001b[37m  " MOVE_TO_COLUMN_40, register_names[i->rd], i->imm);
            TRACE_REGISTER(i->rd);
            break;

        case RISCV_SRLI: case RISCV_SRLIW: case RISCV_SRAI: case RISCV_SRAIW:
        case RISCV_SLLI: case RISCV_SLLIW: case RISCV_SLTI: case RISCV_SLTIU: case RISCV_XORI:
        case RISCV_ORI:  case RISCV_ADDI: case RISCV_ADDIW: case RISCV_ANDI: 
            TRACE_PRINT(opcode_names[i->opcode]);
            TRACE_PRINT(
                "   %s, %s, \u001b[31m#%04llx\u001b[37m" MOVE_TO_COLUMN_40, register_names[i->rd], register_names[i->rs1],
                i->imm);
            TRACE_REGISTER(i->rs1);
            break;

        case RISCV_SLLW: case RISCV_DIV: case RISCV_DIVW: case RISCV_DIVU: case RISCV_DIVUW:
        case RISCV_REM: case RISCV_REMU: case RISCV_SRLW: case RISCV_SRAW: case RISCV_SLL:
        case RISCV_SRL: case RISCV_SRA:  case RISCV_SUB:  case RISCV_SUBW: case RISCV_ADDW:
        case RISCV_MULW: case RISCV_MUL: case RISCV_XOR:  case RISCV_OR:   case RISCV_AND:
        case RISCV_ADD: case RISCV_REMW: case RISCV_REMUW:
            TRACE_PRINT(opcode_names[i->opcode]);
            TRACE_PRINT(
                "   %s, %s, %s" MOVE_TO_COLUMN_40, register_names[i->rd], register_names[i->rs1], register_names[i->rs2]);
            TRACE_REGISTER(i->rs1);
            TRACE_REGISTER(i->rs2);
            break;

        case RISCV_MRET: case RISCV_SRET:
            if (core->has_supervisor_mode)
                TRACE_PRINT("sret   ");
            else
                TRACE_PRINT("mret   ");
            TRACE_ADDRESS(core->xepc);
            TRACE_PRINT("\n");
            break;

        case RISCV_EBREAK: 
            TRACE_PRINT("ebreak \u001b[33m BREAKPOINT TRAP\n");
            break;

        case RISCV_WFI:
            TRACE_PRINT("wfi");
            break;

        case RISCV_ECALL:
            TRACE_PRINT("ecall \u001b[33m ENVIRONMENT TRAP\n");
            break;

        case RISCV_CSRRW: case RISCV_CSRRS: case RISCV_CSRRC: 
            TRACE_PRINT(opcode_names[i->opcode]);
            TRACE_PRINT(
                "  \u001b[31m#%04llx\u001b[37m, %s, %s " MOVE_TO_COLUMN_40, i->csr, register_names[i->rd],
                register_names[i->rs1]);
            TRACE_REGISTER(i->rd);
            TRACE_REGISTER(i->rs1);
            break;

        case RISCV_CSRRWI: case RISCV_CSRRSI: case RISCV_CSRRCI:
            TRACE_PRINT(opcode_names[i->opcode]);
            TRACE_PRINT(
                " \u001b[31m#%04llx\u001b[37m, %s, \u001b[31m#%02llx\u001b[37m " MOVE_TO_COLUMN_40, i->csr,
                register_names[i->rd], i->imm);
            TRACE_REGISTER(i->rd);
            break;

        case RISCV_AUIPC:
            TRACE_PRINT("auipc  %s, \u001b[31m#%04llx\u001b[37m " MOVE_TO_COLUMN_40, register_names[i->rd], i->imm);
            TRACE_REGISTER(i->rd);
            TRACE_ADDRESS(i->imm);
            break;

        case RISCV_JAL:
            printed_symbol_since_last_branch = false;
            TRACE_PRINT("jal    %s, \u001b[31m#%04llx\u001b[37m  " MOVE_TO_COLUMN_40, register_names[i->rd], i->imm);
            TRACE_REGISTER(i->rd);
            TRACE_ADDRESS(i->imm);
            break;

        case RISCV_JALR:
            printed_symbol_since_last_branch = false;
            TRACE_PRINT(
                "jalr   %s, \u001b[31m#%04llx\u001b[37m(%s)  " MOVE_TO_COLUMN_40, register_names[i->rd], i->imm,
                register_names[i->rs1]);
            TRACE_REGISTER(i->rd);
            TRACE_REGISTER(i->rs1);
            TRACE_ADDRESS(core->regs[i->rs1] + i->imm);
            break;

        case RISCV_BEQ: case RISCV_BNE: case RISCV_BLT: case RISCV_BGE: case RISCV_BLTU: case RISCV_BGEU:
            printed_symbol_since_last_branch = false;
            TRACE_PRINT(opcode_names[i->opcode]);
            TRACE_PRINT(
                "    %s, %s, \u001b[31m#%04llx\u001b[37m  " MOVE_TO_COLUMN_40, register_names[i->rs1],
                register_names[i->rs2], i->imm);
            TRACE_REGISTER(i->rs1);
            TRACE_REGISTER(i->rs2);
            TRACE_ADDRESS(i->imm);
            break;
    }
}

void riscv_tracer::print_address(LwU64 address, bool asComment)
{
    const char *symbol, *directory, *filename;
    LwU64 out_line = 0, symbolOffset, out_col, lineOffset;

    std::unordered_map<LwU64, resolve_cache>::iterator i = resolverCache.find(address);
    resolve_cache * p;
    if (i == resolverCache.end())
    {
        resolve_cache entry = {0};
        if (!LibosDebugResolveSymbolToName(&core->firmware->resolver, address, &entry.symbol, &entry.symbolOffset))
            entry.symbol = nullptr;

        if (!LibosDwarfResolveLine(
            &core->firmware->resolver, address, &directory, &entry.filename, &entry.line, &out_col, &lineOffset))
            entry.filename = nullptr;

        p = &resolverCache.insert(std::make_pair(address, entry)).first->second;
    }
    else 
        p = &i->second;

    if (asComment) {
        printed_symbol_since_last_branch = true;
        TRACE_PRINT("\n\u001b[37m%s: \u001b[32m;", core->privilege == privilege_machine ? "M" : core->privilege == privilege_supervisor ? "S" : "U");
    }
    else
        TRACE_PRINT("\u001b[35mtarget:%08llx", address);

    if (!asComment)
        TRACE_PRINT("(");
    if (p->symbol)
        TRACE_PRINT("%s+0x%llx", p->symbol, p->symbolOffset);

    if (p->filename)
        TRACE_PRINT("@ %s:%lld", p->filename, p->line);

    if (!asComment)
        TRACE_PRINT(")");

    if (asComment)
        TRACE_PRINT("\n");
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

void riscv_tracer::handler_instruction_start()
{
    if (!printed_symbol_since_last_branch)
        print_address(core->pc, true);

    LwU64 pa = 0;
    riscv_core::TranslationKind mask;
    core->translate_address(core->pc, pa, mask);

    TRACE_PRINT(
        "\u001b[37m%s: \u001b[36m%08llx/%08llx \u001b[37m", core->privilege == privilege_machine ? "M" : core->privilege == privilege_supervisor ? "S" : "U",
        core->pc, pa);
}

void riscv_tracer::handler_instruction_retired() { TRACE_PRINT("\n"); }

void riscv_tracer::handler_register_written(LwU64 rd) { TRACE_OUT_REGISTER(rd); }


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
