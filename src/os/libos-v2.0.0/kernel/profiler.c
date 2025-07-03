/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "kernel.h"
#include "profiler.h"
#include "task.h"
#include "libos_syscalls.h"
#include "riscv-isa.h"

#include "dev_gsp_riscv_csr_64.h"
#include "dev_riscv_csr_64_addendum.h"

/*!
 * @file profiler.c
 * @brief This module provides machine-level services for setting up
 * the MHPMEVENT counters and making them accessible from user mode.
 */

#if !LIBOS_CONFIG_RISCV_S_MODE
static LwU64 hpmCount[10] = {0,};
static LwBool hpmDataValid = LW_FALSE;
#endif

static LwBool profilingEnabled = LW_FALSE;

/**
 * @brief Called from user mode to enable profiling.
 */
void LIBOS_SECTION_TEXT_COLD kernel_syscall_profiler_enable(void)
{
    profilingEnabled = LW_TRUE;
    kernel_profiler_init();
}

/**
 * @brief Called from user mode to get mask of counters that are enabled.
 */
void LIBOS_SECTION_TEXT_COLD kernel_syscall_profiler_get_counter_mask(void)
{
    self->state.registers[LIBOS_REG_IOCTL_A0] = csr_read(LW_RISCV_CSR_XCOUNTEREN);
}

/**
 * @brief Set up the MHPMEVENT counters and make them accessible from user mode.
 */
void LIBOS_SECTION_TEXT_COLD kernel_profiler_init(void)
{
    if (!profilingEnabled)
        return;

#if LIBOS_CONFIG_RISCV_S_MODE

    // TODO: Call into SK to enable counters, program MHPMEVENT, and get mask.

    // Enable profiling CSRs: cycle, time, and instret.
    csr_set(LW_RISCV_CSR_SCOUNTEREN,
        REF_NUM64(LW_RISCV_CSR_SCOUNTEREN_CY, 1u) |
        REF_NUM64(LW_RISCV_CSR_SCOUNTEREN_TM, 1u) |
        REF_NUM64(LW_RISCV_CSR_SCOUNTEREN_IR, 1u));

#else // LIBOS_CONFIG_RISCV_S_MODE

    // M mode

    // Enable profiling CSRs: cycle, time, instret, hpm3-10 counters.
    csr_set(LW_RISCV_CSR_MUCOUNTEREN,
            REF_NUM64(LW_RISCV_CSR_MUCOUNTEREN_CY, 1u) |
            REF_NUM64(LW_RISCV_CSR_MUCOUNTEREN_TM, 1u) |
            REF_NUM64(LW_RISCV_CSR_MUCOUNTEREN_IR, 1u) |
            REF_NUM64(LW_RISCV_CSR_MUCOUNTEREN_HPM, 0xffu));

    // Initialize HW perf counters
    // subevent = IO read (global)
    csr_write(LW_RISCV_CSR_MHPMEVENT3,
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_USER_MODE, _ENABLE)                   |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_MACHINE_MODE, _ENABLE)                |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_EVENT_INDEX, _DATA_ACCESS)            |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_DATA_ACCESS_PRIV_INTERNAL_READ, 0x1U) |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_DATA_ACCESS_PRIV_EXTERNAL_READ, 0x1U));

    // subevent = IO write (global)
    csr_write(LW_RISCV_CSR_MHPMEVENT4,
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_USER_MODE, _ENABLE)                   |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_MACHINE_MODE, _ENABLE)                |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_EVENT_INDEX, _DATA_ACCESS)            |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_DATA_ACCESS_PRIV_INTERNAL_WRITE, 0x1U) |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_DATA_ACCESS_PRIV_EXTERNAL_WRITE, 0x1U));

    // subevent = HUB read
    csr_write(LW_RISCV_CSR_MHPMEVENT5,
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_USER_MODE, _ENABLE)                   |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_MACHINE_MODE, _ENABLE)                |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_EVENT_INDEX, _DATA_ACCESS)            |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_DATA_ACCESS_HUB_READ, 0x1U));

    // subevent = HUB write
    csr_write(LW_RISCV_CSR_MHPMEVENT6,
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_USER_MODE, _ENABLE)                   |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_MACHINE_MODE, _ENABLE)                |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_EVENT_INDEX, _DATA_ACCESS)            |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_DATA_ACCESS_HUB_WRITE, 0x1U));

    // icache miss
    csr_write(LW_RISCV_CSR_MHPMEVENT7,
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_USER_MODE, _ENABLE)                   |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_MACHINE_MODE, _ENABLE)                |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_EVENT_INDEX, _ICACHE)                 |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_ICACHE_READ_MISS, 0x1U));

    // dcache miss
    csr_write(LW_RISCV_CSR_MHPMEVENT8,
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_USER_MODE, _ENABLE)                   |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_MACHINE_MODE, _ENABLE)                |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_EVENT_INDEX, _DCACHE)                 |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_DCACHE_READ_MISS, 0x1U));

    // branches
    csr_write(LW_RISCV_CSR_MHPMEVENT9,
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_USER_MODE, _ENABLE)                   |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_MACHINE_MODE, _ENABLE)                |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_EVENT_INDEX, _BRANCH)                 |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_BRANCH_J, 0x1U)         |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_BRANCH_JAL, 0x1U)       |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_BRANCH_JR, 0x1U)        |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_BRANCH_JALR, 0x1U)      |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_BRANCH_RET, 0x1U)       |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_BRANCH_INTERRUPT, 0x1U) |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_BRANCH_EXCEPTION, 0x1U) |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_BRANCH_MRET, 0x1U)      |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_BRANCH_BCC, 0x1U));

    // branch mispredict
    csr_write(LW_RISCV_CSR_MHPMEVENT10,
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_USER_MODE, _ENABLE)                   |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_MACHINE_MODE, _ENABLE)                |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_EVENT_INDEX, _BRANCH_PREDICT)         |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_BRANCH_PREDICT_BTB_WRONG, 0x1U) |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_BRANCH_PREDICT_RAS_WRONG, 0x1U) |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_BRANCH_PREDICT_BHT_WRONG, 0x1U));

    // Restore the counters when starting GSP after it is suspended.
    if (hpmDataValid)
    {
        csr_write(LW_RISCV_CSR_MHPMCOUNTER(3),  hpmCount[0]);
        csr_write(LW_RISCV_CSR_MHPMCOUNTER(4),  hpmCount[1]);
        csr_write(LW_RISCV_CSR_MHPMCOUNTER(5),  hpmCount[2]);
        csr_write(LW_RISCV_CSR_MHPMCOUNTER(6),  hpmCount[3]);
        csr_write(LW_RISCV_CSR_MHPMCOUNTER(7),  hpmCount[4]);
        csr_write(LW_RISCV_CSR_MHPMCOUNTER(8),  hpmCount[5]);
        csr_write(LW_RISCV_CSR_MHPMCOUNTER(9),  hpmCount[6]);
        csr_write(LW_RISCV_CSR_MHPMCOUNTER(10), hpmCount[7]);
        csr_write(LW_RISCV_CSR_MCYCLE,          hpmCount[8]);
        csr_write(LW_RISCV_CSR_MINSTRET,        hpmCount[9]);
        hpmDataValid = LW_FALSE;
    }
#endif // LIBOS_CONFIG_RISCV_S_MODE
}

/**
 * @brief Save the MHPMEVENT counters before suspending GSP.
 */
LIBOS_SECTION_TEXT_COLD void kernel_profiler_save_counters(void)
{
#if !LIBOS_CONFIG_RISCV_S_MODE
    if (profilingEnabled)
    {
        hpmCount[0] = csr_read(LW_RISCV_CSR_MHPMCOUNTER(3));
        hpmCount[1] = csr_read(LW_RISCV_CSR_MHPMCOUNTER(4));
        hpmCount[2] = csr_read(LW_RISCV_CSR_MHPMCOUNTER(5));
        hpmCount[3] = csr_read(LW_RISCV_CSR_MHPMCOUNTER(6));
        hpmCount[4] = csr_read(LW_RISCV_CSR_MHPMCOUNTER(7));
        hpmCount[5] = csr_read(LW_RISCV_CSR_MHPMCOUNTER(8));
        hpmCount[6] = csr_read(LW_RISCV_CSR_MHPMCOUNTER(9));
        hpmCount[7] = csr_read(LW_RISCV_CSR_MHPMCOUNTER(10));
        hpmCount[8] = csr_read(LW_RISCV_CSR_MCYCLE);
        hpmCount[9] = csr_read(LW_RISCV_CSR_MINSTRET);
        hpmDataValid = LW_TRUE;
    }
#endif // !LIBOS_CONFIG_RISCV_S_MODE
}
