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

static volatile uint64_t irq_result = PASSING_DEBUG_VALUE;
static void irqHandler_Timer(void)
{
    printf("timer irq ");
    disableTimerInterrupt();
    irq_result = PASSING_DEBUG_VALUE;
}

static void irqHandler_Ext(void)
{
    uint32_t r;
    printf("ext irq ");

    r = localRead(LW_PRGNLCL_FALCON_IRQSTAT);
    if (!FLD_TEST_DRF(_PRGNLCL, _FALCON_IRQSTAT, _SWGEN0, _TRUE, r))
    {
        irq_result = MAKE_DEBUG_FULL(TestId_Irq, 1, 1, DRF_DEF(_PRGNLCL, _FALCON_IRQSTAT, _SWGEN0, _TRUE), r);
        printf("!");
        goto out;
    }
    irq_result = PASSING_DEBUG_VALUE;
out:
    localWrite(LW_PRGNLCL_FALCON_IRQSSET, FLD_SET_DRF(_PRGNLCL, _FALCON_IRQSCLR, _SWGEN0, _SET, localRead(LW_PRGNLCL_FALCON_IRQSSET)));
    disableExtInterrupt();
}

#define MEMERR_TEST_ADDRESS 0x1212340

static void irqHandler_wMemerror(void)
{
    uint32_t r, addr_lo;
    printf("memerr irq ");

    r = localRead(LW_PRGNLCL_RISCV_PRIV_ERR_STAT);
    if (!FLD_TEST_DRF(_PRGNLCL, _RISCV_PRIV_ERR_STAT, _VALID, _TRUE, r))
    {
        printf("FAIL ERR_STAT_VALID ");
        irq_result = MAKE_DEBUG_FULL(TestId_MemErrWrite, 0, 0, DRF_DEF(_PRGNLCL, _RISCV_PRIV_ERR_STAT, _VALID, _TRUE), r);
        goto out;
    }

    if (!FLD_TEST_DRF(_PRGNLCL, _RISCV_PRIV_ERR_STAT, _WRITE, _TRUE, r))
    {
        printf("FAIL ERR_STAT_WRITE ");
        irq_result = MAKE_DEBUG_FULL(TestId_MemErrWrite, 2, 2, DRF_DEF(_PRGNLCL, _RISCV_PRIV_ERR_STAT, _WRITE, _TRUE), r);
        goto out;
    }

    addr_lo = localRead(LW_PRGNLCL_RISCV_PRIV_ERR_ADDR);
    r = localRead(LW_PRGNLCL_RISCV_PRIV_ERR_INFO);
    if (addr_lo != MEMERR_TEST_ADDRESS)
    {
        printf("FAIL ERR_ADDR ");
        irq_result = MAKE_DEBUG_FULL(TestId_MemErrWrite, 2, 3, MEMERR_TEST_ADDRESS, localRead(LW_PRGNLCL_RISCV_PRIV_ERR_ADDR));
        goto out;
    }

    if (r != 0xbadf1100)
    {
        printf("FAIL ERR_INFO");
        irq_result = MAKE_DEBUG_FULL(TestId_MemErrWrite, 2, 3, 0xbadf1100, localRead(LW_PRGNLCL_RISCV_PRIV_ERR_INFO));
        goto out;
    }

    irq_result = PASSING_DEBUG_VALUE;
out:
    disableExtInterrupt();
    localWrite(LW_PRGNLCL_RISCV_PRIV_ERR_STAT, REF_DEF(LW_PRGNLCL_RISCV_PRIV_ERR_STAT_VALID, _FALSE));
    localWrite(LW_PRGNLCL_FALCON_IRQSCLR, REF_DEF(LW_PRGNLCL_FALCON_IRQSCLR_MEMERR, _SET));
}

static void riscvWaitTime(uint32_t us)
{
    uint64_t ticks_wait = NS_TO_TICKS((uint64_t)us * 1000);
    uint64_t end_time = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME) + ticks_wait;

    while (end_time > csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME))
    {}
}

#define TIMER_PERIOD (100 * 1000)
extern void irq_test_handler_m(void);
extern void (*irq_handler_fptr)(void);
uint64_t testIrq_MM(void)
{
    uint64_t old_mtvec;

    old_mtvec = csr_read(LW_RISCV_CSR_MTVEC);
    irq_handler_fptr = irqHandler_Timer;
    csr_write(LW_RISCV_CSR_MTVEC, (uintptr_t)irq_test_handler_m);

    irq_result = MAKE_DEBUG_NOVAL(TestId_Irq, 0, 0);
    enableTimerInterrupt();
    unmaskInterrupts();
    printf("Testing time interrupt M-M ...");
    setTimerInterrupt(10 * 1000);
    riscvWaitTime(12 * 1000);
    // We need to wait, because FMODEL doesn't get the interrupt
    //    wfi();
    printf("done.\n");
    disableTimerInterrupt();
    if (irq_result != PASSING_DEBUG_VALUE)
        goto end;

    irq_result = MAKE_DEBUG_NOVAL(TestId_Irq, 1, 0);
    irq_handler_fptr = irqHandler_Ext;
    // Test SWGEN0
    {
        LwU32 irqDest = 0U;
        LwU32 irqMset = 0U;
        LwU32 irqMode = localRead(LW_PRGNLCL_FALCON_IRQMODE);

        printf("Testing swgen0 interrupt M-M ...");
        irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _SWGEN0, _RISCV, irqDest);
        irqMset = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _SWGEN0, _SET, irqMset);
        localWrite(LW_PRGNLCL_FALCON_IRQMODE, irqMode);
        localWrite(LW_PRGNLCL_RISCV_IRQDEST, irqDest);
        localWrite(LW_PRGNLCL_RISCV_IRQMSET, irqMset);
        localWrite(LW_PRGNLCL_RISCV_IRQMCLR, ~irqMset);
        localWrite(LW_PRGNLCL_RISCV_IRQDELEG, 0); // route to M mode

        enableExtInterrupt();
            localWrite(LW_PRGNLCL_FALCON_IRQSSET, FLD_SET_DRF(_PRGNLCL, _FALCON_IRQSSET, _SWGEN0, _SET, localRead(LW_PRGNLCL_FALCON_IRQSSET)));
            riscvLwfenceIO();
            if (irq_result != PASSING_DEBUG_VALUE)
                riscvWaitTime(1000); // 1ms is enough
        localWrite(LW_PRGNLCL_FALCON_IRQMODE, 0);
        localWrite(LW_PRGNLCL_RISCV_IRQDEST, 0);
        localWrite(LW_PRGNLCL_RISCV_IRQMCLR, irqMset);
        localWrite(LW_PRGNLCL_RISCV_IRQDELEG, 0);
        printf("done.\n");
        disableExtInterrupt();
        if (irq_result != PASSING_DEBUG_VALUE)
            goto end;
    }

#if LWRISCV_HAS_PRI
    irq_result = MAKE_DEBUG_NOVAL(TestId_Irq, 2, 0);
    irq_handler_fptr = irqHandler_wMemerror;
    {
        LwU32 irqDest = 0U;
        LwU32 irqMset = 0U;
        LwU32 irqMode = localRead(LW_PRGNLCL_FALCON_IRQMODE);

        printf("Testing MEMERR interrupt M-M ...");
        csr_clear(LW_RISCV_CSR_MCFG, 1<<15);
        irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _MEMERR, _RISCV, irqDest);
        irqMset = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _MEMERR, _SET, irqMset);
        localWrite(LW_PRGNLCL_FALCON_IRQMODE, irqMode);
        localWrite(LW_PRGNLCL_RISCV_IRQDEST, irqDest);
        localWrite(LW_PRGNLCL_RISCV_IRQMSET, irqMset);
        localWrite(LW_PRGNLCL_RISCV_IRQMCLR, ~irqMset);
        localWrite(LW_PRGNLCL_RISCV_IRQDELEG, 0); // route to M mode
        localWrite(LW_PRGNLCL_RISCV_PRIV_ERR_STAT, 0);

        enableExtInterrupt();
        priWrite(MEMERR_TEST_ADDRESS, 0);
        riscvFenceIO();

        if (irq_result != PASSING_DEBUG_VALUE)
            riscvWaitTime(10 * 1000); // 10ms should be enough

        localWrite(LW_PRGNLCL_RISCV_PRIV_ERR_STAT, 0);
        localWrite(LW_PRGNLCL_FALCON_IRQMODE, 0);
        localWrite(LW_PRGNLCL_RISCV_IRQDEST, 0);
        localWrite(LW_PRGNLCL_RISCV_IRQMSET, 0);
        localWrite(LW_PRGNLCL_RISCV_IRQMCLR, irqMset);
        localWrite(LW_PRGNLCL_RISCV_IRQDELEG, 0);
        printf("done.\n");
        disableExtInterrupt();
        if (irq_result != PASSING_DEBUG_VALUE)
            goto end;
    }
    if (irq_result != PASSING_DEBUG_VALUE)
        goto end;
#endif

end:
    maskInterrupts();
    csr_write(LW_RISCV_CSR_MTVEC, old_mtvec);
    return irq_result;
}
