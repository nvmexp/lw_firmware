#pragma once
#include "libriscv.h"
#include "lwmisc.h"
#include "gsp.h"
#include <assert.h>
#include <turing/tu102/dev_gsp_riscv_csr_64.h>
#include <iostream>
#include <memory.h>
#include <lwtypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unordered_map>
#include <vector>

/*
    Challenges in dynamic stack validation
        - SP can be reset through lui/addi or auipc/addi
        - We only care about the stack when opreating at privilege == machine
        - We re-initialize the stack when preparing to switch back to user-space
          (hmm)
*/

struct validator_stack_depth
{
    ::gsp *gsp;
    enum
    {
        state_ilwalid,
        state_exelwting_sp_set,
        state_waiting_for_addi,
        state_valid
    } state;
    uint64_t stack_top;
    validator_stack_depth(::gsp *core) : gsp(core)
    {
        core->event_instruction_decoded.connect(&validator_stack_depth::event_instruction_decoded, this);        
        core->event_instruction_retired.connect(&validator_stack_depth::handler_instruction_retired, this);
    }

    void event_instruction_decoded(LwU64 pc, LibosRiscvInstruction * i)
    {
        switch(i->opcode)
        {
            case RISCV_SLLW: case RISCV_DIV: case RISCV_DIVW: case RISCV_DIVU: case RISCV_DIVUW:
            case RISCV_REM: case RISCV_REMU: case RISCV_SRLW: case RISCV_SRAW: case RISCV_SLL:
            case RISCV_SRL: case RISCV_SRA:  case RISCV_SUB:  case RISCV_SUBW: case RISCV_ADDW:
            case RISCV_MULW: case RISCV_MUL: case RISCV_XOR:  case RISCV_OR:   case RISCV_AND:
            case RISCV_ADD: case RISCV_REMW: case RISCV_REMUW:
            {
                // We only care about kernel stack
                if (gsp->privilege != privilege_machine || i->rd != RISCV_SP)
                    return;

                assert(0 && "Unsupported stack operation");
                break;
            }

            case RISCV_SRLI: case RISCV_SRLIW: case RISCV_SRAI: case RISCV_SRAIW:
            case RISCV_SLLI: case RISCV_SLLIW: case RISCV_SLTI: case RISCV_SLTIU: case RISCV_XORI:
            case RISCV_ORI:  case RISCV_ADDI: case RISCV_ADDIW: case RISCV_ANDI: 
            {
                // We only care about kernel stack
                if (gsp->privilege != privilege_machine || i->rd != RISCV_SP || i->opcode != RISCV_ADDI)
                    return;

                // Only allowed for the very next instruction
                LwU64 new_value = gsp->regs[i->rs1] + i->imm;
                if (state == state_waiting_for_addi)
                    state = state_valid, stack_top = new_value;
                else if (state == state_valid)
                {
                    if ((stack_top - new_value) > LIBOS_CONFIG_KERNEL_STACK_SIZE)
                    {
                        printf("Stack overflow! Current stack depth exceeds limit : %lld \n", stack_top - new_value);
                        printf("Please update LIBOS_KERNE_STACK_SIZE\n");
                        exit(1);
                    }
                }
                else
                    assert(0 && "Operation on un-initialized stack");            
                break;
            }

            case RISCV_AUIPC:
                // We only care about kernel stack
                if (gsp->privilege != privilege_machine || i->rd != RISCV_SP)
                    return;

                state = state_exelwting_sp_set;
                break;

            case RISCV_LUI:
            case RISCV_LI:
                // We only care about kernel stack
                if (gsp->privilege != privilege_machine || i->rd != RISCV_SP)
                    return;

                state = state_exelwting_sp_set;            
                break;
        }
    }

    void handler_trap(LwU64 new_mcause, LwU64 new_mbadaddr) { state = state_ilwalid; }

    void handler_instruction_retired()
    {
        // We only care about kernel stack
        if (gsp->privilege != privilege_machine)
            return;

        if (state == state_exelwting_sp_set)
            state = state_waiting_for_addi;
        else if (state == state_waiting_for_addi)
            state = state_valid, stack_top = gsp->regs[RISCV_SP];
    }
};