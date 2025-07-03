/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#pragma once

#include "riscv-core.h"

struct riscv_tracer
{
    riscv_core *core;
    bool printed_symbol_since_last_branch = false;
    riscv_tracer(riscv_core *core);

    void print_address(LwU64 address);
    void print_register(LwU32 r);
    void print_out_register(LwU32 r);
    void handler_store(store_kind kind, LwU64 rs1, LwU64 rs2, LwS64 imm);
    void handler_load(load_kind kind, LwU64 rd, LwU64 rs1, LwS64 imm);
    void handler_instruction_retired();
    void handler_instruction_start();
    void handler_register_written(LwU64 rd);
    void handler_arithmetic(arithmetic_kind kind, LwU64 rd, LwU64 rs1, LwU64 rs2);
    void handler_arith_immediate(arithmetic_imm_kind kind, LwU64 rd, LwU64 rs1, LwS64 imm);
    void handler_fast_forward(LwU64 new_cycle);
    void handler_trap(LwU64 new_mcause, LwU64 new_mbadaddr);
    void handler_auipc(LwU64 rd, LwU64 target);
    void handler_lui(LwU64 rd, LwU64 imm);
    void handler_jalr(LwU64 rd, LwU64 rs1, LwS64 imm);
    void handler_jal(LwU64 rd, LwU64 target);
    void handler_branch(branch_kind kind, LwU64 rs1, LwU64 rs2, LwU64 target);
    void handler_csr(csr_kind kind, LwU64 csr, LwU64 rd, LwU64 rs1);
    void handler_csri(csr_kind kind, LwU64 csr, LwU64 rd, LwU64 imm);
    void handler_ecall();
    void handler_wfi();
    void handler_ebreak();
    void handler_mret();
};