#include <lw_riscv_address_map.h>
#include <stdint.h>
#include <lwmisc.h>
#include <liblwriscv/libc.h>

#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <test.h>

int partition_0_main(void)
{
    TEST_INIT("SBI_SHUTDOWN");

    // test 1 -> test invalid function id
    TEST(SBI_EXTENSION_SHUTDOWN, 1, 0);

    // test 2 -> test normal case -> shutdown the core
    TEST(SBI_EXTENSION_SHUTDOWN, 0, 0);
    printf("not_expecting");

    TEST_END();

    return 0;
}

void partition_0_trap_handler(void)
{

    uint64_t scause = csr_read(LW_RISCV_CSR_SCAUSE);
    uint64_t svalue = csr_read(LW_RISCV_CSR_STVAL);
    TEST_FAILED("Trap handler. cause: 0x%lx, value: 0x%lx\n", scause, svalue);
    return;
}
