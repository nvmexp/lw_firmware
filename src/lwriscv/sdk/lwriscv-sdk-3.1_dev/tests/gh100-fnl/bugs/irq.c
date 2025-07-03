/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <stdint.h>
#include <lwmisc.h>
#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <liblwriscv/shutdown.h>
#include <lwriscv/fence.h>

#include "tests.h"
#include "util.h"
#define TICK_SCALE 5

#define TICKS_TO_NS(X) ((X) << TICK_SCALE)
#define NS_TO_TICKS(X) ((X) >> TICK_SCALE)

static inline void wfi(void)
{
    __asm__ volatile ("wfi");
}

static void maskInterrupts(void)
{
    csr_clear(LW_RISCV_CSR_XSTATUS, DRF_DEF64(_RISCV, _CSR_XSTATUS, _XIE, _ENABLE));
}

static void unmaskInterrupts(void)
{
    csr_set(LW_RISCV_CSR_XSTATUS, DRF_DEF64(_RISCV, _CSR_XSTATUS, _XIE, _ENABLE));
}

// Timer interrupt, for testing
static void enableTimerInterrupt(void)
{
    csr_set(LW_RISCV_CSR_XIE, DRF_NUM64(_RISCV, _CSR_XIE, _XTIE, 1));
}

static void disableTimerInterrupt(void)
{
    csr_clear(LW_RISCV_CSR_XIE, DRF_NUM64(_RISCV, _CSR_XIE, _XTIE, 1));
}

static void setTimerInterrupt(uint64_t deltaUs)
{
    deltaUs = NS_TO_TICKS(deltaUs * 1000);
    csr_write( LW_RISCV_CSR_MTIMECMP, csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME) + deltaUs);
}

static bool timerInterruptPending(void)
{
    // MK TODO: we need XIP in csr.h
    return FLD_TEST_DRF_NUM64(_RISCV, _CSR_MIP, _MTIP, 1, csr_read(LW_RISCV_CSR_MIP));
}

// External interrupts
static void enableExtInterrupt(void)
{
    csr_set(LW_RISCV_CSR_XIE, DRF_NUM64(_RISCV, _CSR_XIE, _XEIE, 1));
}

static void disableExtInterrupt(void)
{
    csr_clear(LW_RISCV_CSR_XIE, DRF_NUM64(_RISCV, _CSR_XIE, _XEIE, 1));
}

static bool extInterruptPending(void)
{
    // MK TODO: we need XIP in csr.h
    return FLD_TEST_DRF_NUM64(_RISCV, _CSR_MIP, _MEIP, 1, csr_read(LW_RISCV_CSR_MIP));
}

static void riscvWaitTime(uint32_t us)
{
    uint64_t ticks_wait = NS_TO_TICKS((uint64_t)us * 1000);
    uint64_t end_time = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME) + ticks_wait;

    while (end_time > csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME))
    {}
}
