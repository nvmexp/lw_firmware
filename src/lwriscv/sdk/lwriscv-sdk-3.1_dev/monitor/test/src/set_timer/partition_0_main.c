#include <lw_riscv_address_map.h>
#include <stdint.h>
#include <stdbool.h>
#include <lwmisc.h>
#include <liblwriscv/libc.h>

#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <test.h>

#define TIME_CSR (LW_RISCV_CSR_HPMCOUNTER_TIME)
#define TIMER_INCR (100)
#define TIMER_WAIT (TIMER_INCR + 100)

static void sleep(unsigned wait)
{
    uint64_t r = csr_read(TIME_CSR) + wait ;
    uint64_t a = 0;

    while (a < r)
    {
        a = csr_read(TIME_CSR);
    }
}

static void timer_interrupt_setup(bool enable)
{
    if (enable)
    {
        csr_set(LW_RISCV_CSR_SIE, DRF_NUM64(_RISCV, _CSR_SIE, _STIE, 1));
        csr_set(LW_RISCV_CSR_SSTATUS, DRF_DEF64(_RISCV, _CSR_SSTATUS, _SIE, _ENABLE));
    }
    else
    {
        csr_set(LW_RISCV_CSR_SIE, DRF_NUM64(_RISCV, _CSR_SIE, _STIE, 0));
        csr_set(LW_RISCV_CSR_SSTATUS, DRF_DEF64(_RISCV, _CSR_SSTATUS, _SIE, _DISABLE));
    }
}

int partition_0_main(void)
{
    TEST_INIT("SBI_SET_TIMER");

    // test 1 -> normal case
    // for normal case test, also test to make sure interrupt is delivered
    TEST(SBI_EXTENSION_SET_TIMER, 0, 1, (csr_read( TIME_CSR ) + TIMER_INCR));
    TEST_EXPECT(TIMER_INT);
    timer_interrupt_setup(true);
    sleep(TIMER_WAIT);

    // test 2 -> invalid function id
    TEST(SBI_EXTENSION_SET_TIMER, 1, 1, (csr_read( TIME_CSR ) + TIMER_INCR));

    TEST_END();

    sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);

    return 0;
}

void partition_0_trap_handler(void)
{
    uint64_t scause = csr_read(LW_RISCV_CSR_SCAUSE);

    if (DRF_VAL64(_RISCV_CSR, _SCAUSE, _INT, scause))
    {
        if (DRF_VAL64(_RISCV_CSR, _SCAUSE, _EXCODE, scause) == LW_RISCV_CSR_SCAUSE_EXCODE_S_TINT)
        {
            TIMER_INT_DELIVER;

            // clear SIP call Set Timer SBI with very large value
            sbicall1(SBI_EXTENSION_SET_TIMER, 0, LW_U64_MAX );

            // disable timer interrupt
            timer_interrupt_setup(false);
            return;
        }
    }

    uint64_t svalue = csr_read(LW_RISCV_CSR_STVAL);
    TEST_FAILED("Trap handler. cause: 0x%lx, value: 0x%lx\n", scause, svalue);
    return;
}
