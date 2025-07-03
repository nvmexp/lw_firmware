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

int partition_0_main(void)
{
    TEST_INIT("SBI_UPDATE_TRACECTL");

    uint64_t tracectl = DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _LOW_THSHD, _INIT) |
                        DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _HIGH_THSHD, _INIT) |
                        DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _UMODE_ENABLE, _TRUE) |
                        DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _SMODE_ENABLE, _TRUE) |
                        DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _MODE, _STACK);

    // test 1 -> normal case
    TEST(SBI_EXTENSION_LWIDIA, 2, 1, tracectl);

    // test 2 -> test for invalid size
    TEST(SBI_EXTENSION_LWIDIA, 2, 1, tracectl | 0x100000000ULL);

    // test 3 -> test for invalid mode
    // mode field is 2bit wide but only valid for 0, 1 or 2.
    TEST(SBI_EXTENSION_LWIDIA, 2, 1, tracectl | (0x3 << 24));

    TEST_END();

    sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);

    return 0;
}

void partition_0_trap_handler(void)
{
    uint64_t scause = csr_read(LW_RISCV_CSR_SCAUSE);
    uint64_t svalue = csr_read(LW_RISCV_CSR_STVAL);
    TEST_FAILED("Trap handler. cause: 0x%lx, value: 0x%lx\n", scause, svalue);
    return;
}
