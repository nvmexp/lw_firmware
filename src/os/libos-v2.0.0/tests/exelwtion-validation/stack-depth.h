#pragma once
#include "../../kernel/libos_defines.h"
#include "kernel/riscv-isa.h"
#include "sdk/lwpu/inc/lwmisc.h"
#include "simulator/gsp.h"
#include <assert.h>
#include <drivers/common/inc/hwref/turing/tu102/dev_gsp_riscv_csr_64.h>
#include <drivers/common/inc/swref/turing/tu102/dev_riscv_csr_64_addendum.h>
#include <iostream>
#include <memory.h>
#include <sdk/lwpu/inc/lwtypes.h>
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
        core->event_lui.connect(&validator_stack_depth::handler_lui, this);
        core->event_auipc.connect(&validator_stack_depth::handler_auipc, this);
        core->event_arith_immediate.connect(&validator_stack_depth::handler_arith_immediate, this);
        core->event_arithmetic.connect(&validator_stack_depth::handler_arithmetic, this);
        core->event_instruction_retired.connect(&validator_stack_depth::handler_instruction_retired, this);
    }

    void handler_trap(LwU64 new_mcause, LwU64 new_mbadaddr) { state = state_ilwalid; }

    void handler_arithmetic(arithmetic_kind kind, LwU64 rd, LwU64 rs1, LwU64 rs2)
    {
        // We only care about kernel stack
        if (gsp->privilege != privilege_machine || rd != RISCV_SP)
            return;

        assert(0 && "Unsupported stack operation");
    }

    void handler_arith_immediate(arithmetic_imm_kind kind, LwU64 rd, LwU64 rs1, LwS64 imm)
    {
        // We only care about kernel stack
        if (gsp->privilege != privilege_machine || rd != RISCV_SP || kind != ADDI)
            return;

        // Only allowed for the very next instruction
        LwU64 new_value = gsp->regs[rs1] + imm;
        if (state == state_waiting_for_addi)
            state = state_valid, stack_top = new_value;
        else if (state == state_valid)
        {
            if ((stack_top - new_value) > LIBOS_KERNEL_STACK_SIZE)
            {
                printf("Stack overflow! Current stack depth exceeds limit : %lld \n", stack_top - new_value);
                printf("Please update LIBOS_KERNE_STACK_SIZE\n");
                exit(1);
            }
        }
        else
            assert(0 && "Operation on un-initialized stack");
    }

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

    void handler_auipc(LwU64 rd, LwU64 target)
    {
        // We only care about kernel stack
        if (gsp->privilege != privilege_machine || rd != RISCV_SP)
            return;

        state = state_exelwting_sp_set;
    }

    void handler_lui(LwU64 rd, LwU64 imm)
    {
        // We only care about kernel stack
        if (gsp->privilege != privilege_machine || rd != RISCV_SP)
            return;

        state = state_exelwting_sp_set;
    }
};
