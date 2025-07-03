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
#include <unordered_map>

struct riscv_tracer
{
    struct resolve_cache
    {
        const char * symbol;
        LwU64        symbolOffset;
        const char * filename;
        LwU64        line;
    };
    std::unordered_map<LwU64, resolve_cache> resolverCache;

    riscv_core *core;
    bool printed_symbol_since_last_branch = false;
    riscv_tracer(riscv_core *core);

    void print_address(LwU64 address, bool asAddress);
    void print_register(LwU32 r);
    void print_out_register(LwU32 r);
    void handler_instruction_retired();
    void handler_instruction_start();
    void handler_register_written(LwU64 rd);
    void handler_fast_forward(LwU64 new_cycle);
    void handler_trap(LwU64 new_mcause, LwU64 new_mbadaddr);
    void instruction_decoded(LwU64 pc, LibosRiscvInstruction * i);
};