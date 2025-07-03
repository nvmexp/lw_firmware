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
    TEST_INIT("SBI_FBIF_REGIONCFG");

    // arbitrary value
    uint64_t regioncfg = 0xA;

    // test 1 -> normal case, test all regions
    for (uint64_t region = 0; region < 8; region++)
    {
        TEST(SBI_EXTENSION_LWIDIA, 4, 2, region, regioncfg);
    }

    // test 2 -> test for invalid size (4bit wide value)
    TEST(SBI_EXTENSION_LWIDIA, 4, 2, 0, regioncfg | 0x10ULL);

    // test 3 -> test for invalid region
    TEST(SBI_EXTENSION_LWIDIA, 4, 2, 8, regioncfg);

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
