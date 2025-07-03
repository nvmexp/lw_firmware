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
    TEST_INIT("SBI_FBIF_TRANSCFG");

    uint64_t transcfg = DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _TARGET, _COHERENT_SYSMEM) |
                        DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
                        DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _L2C_WR, _INIT) |
                        DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _L2C_RD, _INIT);
                        

    // test 1 -> normal case, test all regions
    for (uint64_t region = 0; region < 8; region++)
    {
        TEST(SBI_EXTENSION_LWIDIA, 3, 2, region, transcfg);
    }

    // test 2 -> test for invalid size
    TEST(SBI_EXTENSION_LWIDIA, 3, 2, 0, transcfg | 0x100000000ULL);

    // test 3 -> test for invalid region
    TEST(SBI_EXTENSION_LWIDIA, 3, 2, 8, transcfg);

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
