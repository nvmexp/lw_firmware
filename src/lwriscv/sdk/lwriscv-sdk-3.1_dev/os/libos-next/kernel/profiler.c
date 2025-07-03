/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwmisc.h"
#include "lwtypes.h"
#include "kernel.h"
#include "profiler.h"
#include "lwriscv.h"

/*!
 * @file profiler.c
 * @brief This module provides machine-level services for setting up
 * the MHPMEVENT counters and making them accessible from user mode.
 */

#if LIBOS_LWRISCV < LIBOS_LWRISCV_2_0
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
    ProfilerInit();
}

/**
 * @brief Set up the MHPMEVENT counters and make them accessible from user mode.
 */
void LIBOS_SECTION_TEXT_COLD ProfilerInit(void)
{
#if LIBOS_LWRISCV < LIBOS_LWRISCV_2_0
    if (!profilingEnabled)
        return;

    // Enable profiling CSRs: cycle, time, instret, hpm3-10 counters.
    KernelCsrSet(LW_RISCV_CSR_MUCOUNTEREN,
            REF_NUM64(LW_RISCV_CSR_MUCOUNTEREN_CY, 1u) |
            REF_NUM64(LW_RISCV_CSR_MUCOUNTEREN_TM, 1u) |
            REF_NUM64(LW_RISCV_CSR_MUCOUNTEREN_IR, 1u) |
            REF_NUM64(LW_RISCV_CSR_MUCOUNTEREN_HPM, 0xffu));

    // Initialize HW perf counters
    // subevent = IO read (global)
    KernelCsrWrite(LW_RISCV_CSR_MHPMEVENT3,
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_USER_MODE, _ENABLE)                   |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_MACHINE_MODE, _ENABLE)                |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_EVENT_INDEX, _DATA_ACCESS)            |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_DATA_ACCESS_PRIV_INTERNAL_READ, 0x1U) |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_DATA_ACCESS_PRIV_EXTERNAL_READ, 0x1U));

    // subevent = IO write (global)
    KernelCsrWrite(LW_RISCV_CSR_MHPMEVENT4,
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_USER_MODE, _ENABLE)                   |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_MACHINE_MODE, _ENABLE)                |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_EVENT_INDEX, _DATA_ACCESS)            |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_DATA_ACCESS_PRIV_INTERNAL_WRITE, 0x1U) |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_DATA_ACCESS_PRIV_EXTERNAL_WRITE, 0x1U));

    // subevent = HUB read
    KernelCsrWrite(LW_RISCV_CSR_MHPMEVENT5,
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_USER_MODE, _ENABLE)                   |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_MACHINE_MODE, _ENABLE)                |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_EVENT_INDEX, _DATA_ACCESS)            |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_DATA_ACCESS_HUB_READ, 0x1U));

    // subevent = HUB write
    KernelCsrWrite(LW_RISCV_CSR_MHPMEVENT6,
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_USER_MODE, _ENABLE)                   |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_MACHINE_MODE, _ENABLE)                |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_EVENT_INDEX, _DATA_ACCESS)            |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_DATA_ACCESS_HUB_WRITE, 0x1U));

    // icache miss
    KernelCsrWrite(LW_RISCV_CSR_MHPMEVENT7,
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_USER_MODE, _ENABLE)                   |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_MACHINE_MODE, _ENABLE)                |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_EVENT_INDEX, _ICACHE)                 |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_ICACHE_READ_MISS, 0x1U));

    // dcache miss
    KernelCsrWrite(LW_RISCV_CSR_MHPMEVENT8,
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_USER_MODE, _ENABLE)                   |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_MACHINE_MODE, _ENABLE)                |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_EVENT_INDEX, _DCACHE)                 |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_DCACHE_READ_MISS, 0x1U));

    // branches
    KernelCsrWrite(LW_RISCV_CSR_MHPMEVENT9,
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
    KernelCsrWrite(LW_RISCV_CSR_MHPMEVENT10,
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_USER_MODE, _ENABLE)                   |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_MACHINE_MODE, _ENABLE)                |
        REF_DEF64(LW_RISCV_CSR_MHPMEVENT_EVENT_INDEX, _BRANCH_PREDICT)         |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_BRANCH_PREDICT_BTB_WRONG, 0x1U) |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_BRANCH_PREDICT_RAS_WRONG, 0x1U) |
        REF_NUM64(LW_RISCV_CSR_MHPMEVENT_SUBEVENT_MASK_BRANCH_PREDICT_BHT_WRONG, 0x1U));

    // Restore the counters when starting GSP after it is suspended.
    if (hpmDataValid)
    {
        KernelCsrWrite(LW_RISCV_CSR_MHPMCOUNTER(3),  hpmCount[0]);
        KernelCsrWrite(LW_RISCV_CSR_MHPMCOUNTER(4),  hpmCount[1]);
        KernelCsrWrite(LW_RISCV_CSR_MHPMCOUNTER(5),  hpmCount[2]);
        KernelCsrWrite(LW_RISCV_CSR_MHPMCOUNTER(6),  hpmCount[3]);
        KernelCsrWrite(LW_RISCV_CSR_MHPMCOUNTER(7),  hpmCount[4]);
        KernelCsrWrite(LW_RISCV_CSR_MHPMCOUNTER(8),  hpmCount[5]);
        KernelCsrWrite(LW_RISCV_CSR_MHPMCOUNTER(9),  hpmCount[6]);
        KernelCsrWrite(LW_RISCV_CSR_MHPMCOUNTER(10), hpmCount[7]);
        KernelCsrWrite(LW_RISCV_CSR_MCYCLE,          hpmCount[8]);
        KernelCsrWrite(LW_RISCV_CSR_MINSTRET,        hpmCount[9]);
        hpmDataValid = LW_FALSE;
    }
    #endif
}

/**
 * @brief Save the MHPMEVENT counters before suspending GSP.
 */
LIBOS_SECTION_TEXT_COLD void KernelProfilerSaveCounters(void)
{
#if LIBOS_LWRISCV < LIBOS_LWRISCV_2_0
    if (profilingEnabled)
    {
        hpmCount[0] = KernelCsrRead(LW_RISCV_CSR_MHPMCOUNTER(3));
        hpmCount[1] = KernelCsrRead(LW_RISCV_CSR_MHPMCOUNTER(4));
        hpmCount[2] = KernelCsrRead(LW_RISCV_CSR_MHPMCOUNTER(5));
        hpmCount[3] = KernelCsrRead(LW_RISCV_CSR_MHPMCOUNTER(6));
        hpmCount[4] = KernelCsrRead(LW_RISCV_CSR_MHPMCOUNTER(7));
        hpmCount[5] = KernelCsrRead(LW_RISCV_CSR_MHPMCOUNTER(8));
        hpmCount[6] = KernelCsrRead(LW_RISCV_CSR_MHPMCOUNTER(9));
        hpmCount[7] = KernelCsrRead(LW_RISCV_CSR_MHPMCOUNTER(10));
        hpmCount[8] = KernelCsrRead(LW_RISCV_CSR_MCYCLE);
        hpmCount[9] = KernelCsrRead(LW_RISCV_CSR_MINSTRET);
        hpmDataValid = LW_TRUE;
    }
#endif
}
