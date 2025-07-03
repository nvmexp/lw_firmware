#pragma once
#include "libriscv.h"
#include "lwmisc.h"
#include "simulator/gsp.h"
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

struct validator_register_preservation
{
    ::gsp *gsp;

    struct taskState
    {
        LwBool first_mret    = LW_TRUE;
        LwU32  lwrrent_ioctl = 0;
        LwU64  regs[32];
    };
    std::unordered_map<LwU64, taskState> task_state_map;

    LwU64 kernel_trap_registers[32];
    LwU64 trap_mcause[2];
    LwU64 kernel_mscratch;
    int   trap_depth = 1; // We start in kernel

    validator_register_preservation(::gsp *gsp) : gsp(gsp)
    {
        gsp->event_mret.connect(&validator_register_preservation::handler_mret, this);
        gsp->event_ecall.connect(&validator_register_preservation::handler_ecall, this);
        gsp->event_trap.connect(&validator_register_preservation::handler_trap, this);
    }

    virtual void handler_mret();
    virtual void handler_ecall();
    virtual void handler_trap(LwU64 new_mcause, LwU64 new_mbadaddr);
};
