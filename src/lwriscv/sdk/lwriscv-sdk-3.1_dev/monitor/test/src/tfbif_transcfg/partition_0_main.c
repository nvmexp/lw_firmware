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
    TEST_INIT("SBI_TFBIF_TRANSCFG");

    uint64_t swid = 3;

    // test 1 -> normal case, test all regions
    for (uint64_t region = 0; region < 8; region++)
    {
        TEST(SBI_EXTENSION_LWIDIA, 5, 2, region, swid);
    }

    // test 2 -> test for invalid size
    TEST(SBI_EXTENSION_LWIDIA, 5, 2, 0, swid | 0x4ULL);

    // test 3 -> test for invalid region
    TEST(SBI_EXTENSION_LWIDIA, 5, 2, 8, swid);

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
