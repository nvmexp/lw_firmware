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
    TEST_INIT("SBI_TFBIF_REGIONCFG");

    // arbitrary value
    uint64_t vpr = 0x1;
    uint64_t apert = 0x1F;

    // test 1 -> normal case, test all regions
    for (uint64_t region = 0; region < 8; region++)
    {
        TEST(SBI_EXTENSION_LWIDIA, 6, 3, region, vpr, apert);
    }

    // test 2 -> test for invalid vpr (1bit wide value)
    TEST(SBI_EXTENSION_LWIDIA, 6, 3, 0, vpr | 0x2ULL, apert);

    // test 3 -> test for invalid apert (5bit wide value)
    TEST(SBI_EXTENSION_LWIDIA, 6, 3, 0,  vpr, apert | 0x20ULL);

    // test 4 -> test for invalid region
    TEST(SBI_EXTENSION_LWIDIA, 6, 3, 8, vpr, apert);

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
