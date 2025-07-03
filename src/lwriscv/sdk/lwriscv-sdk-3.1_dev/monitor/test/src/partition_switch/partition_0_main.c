#include <lw_riscv_address_map.h>
#include <stdint.h>
#include <lwmisc.h>
#include <liblwriscv/libc.h>

#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <test.h>
#include <switch_to_param.h>

#define PARTITION_1_ID 1

static int entry = 0;

int partition_0_main(void)
{
    if (entry == 0)
    {
        TEST_INIT("TEST_SWITCH_TO");

        entry++;

        // Test switch to valid partition 1
        TEST(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_FIRST, NUM_SBI_PARAMS,
                PARAM_0, PARAM_1, PARAM_2, PARAM_3, PARAM_4, PARTITION_1_ID);
    }
    else
    {
        SWITCHED(0);

        // Test invalid partition id
        TEST(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_FIRST, NUM_SBI_PARAMS,
                PARAM_0, PARAM_1, PARAM_2, PARAM_3, PARAM_4, 2);
    }

    return 0;
}

void partition_0_trap_handler(void)
{
    uint64_t scause = csr_read(LW_RISCV_CSR_SCAUSE);
    uint64_t svalue = csr_read(LW_RISCV_CSR_STVAL);
    TEST_FAILED("Partition 0 Trap handler. cause: 0x%lx, value: 0x%lx\n", scause, svalue);
    return;
}
